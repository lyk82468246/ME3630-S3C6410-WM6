# V7 synchronous RX/TX concurrency

V7 inherits the V6 serial subset, bounded read-only ADB boot probe, exact USB
19d2:1432 matching and COM1/COM5 assignment. V6 remains the installer fallback.
No LeadCore, WMDC, RF, APN or SIM configuration changes are made.

Each port now has independent RX and TX locks. A third configuration lock
protects DCB/timeouts/control lines. Each read/write snapshots its timeout under
that lock, then releases it before waiting on USB. Timeout changes affect the
next operation, not one already pending. RX staging/remainder state belongs to
the RX lock; TX uses its own local staging. USB endpoint-zero control requests
remain serialized by the configuration lock.

Removal first signals the parent stop event. Pending waits exit; cleanup takes
RX, then TX, then configuration locks before closing either pipe. Each I/O path
takes only its direction lock and briefly the configuration lock, never the
opposite direction lock. Existing context references retain removed open handles
until Close. This is not exhaustive proof of all removal/concurrency races.

`build_com.ps1 -Version7` builds distinct DLL/installer/test files.
`deploy_com.ps1 -Version7` installs through Joe's actual 32-bit WMDC context;
`-Version7 -Test` runs the native tests. A 40-second port-availability deadline
allows temporary enumeration loss. The host reads the result after 50 seconds.
If a module reboot interrupts a round, save its failed evidence and retry after
re-enumeration; do not count previous COMPLETE output as a new test.
The deployment script now waits for a V7 activation marker before launching,
and rejects unchanged/missing test start timestamps. A historical READY marker
alone is not conclusive proof of the current active binary; correlate the COM
open and test timestamps with this version's driver log during verification.

In addition to the 32 existing checks, each port adds six concurrency checks:
set timeouts, create a reader, query configuration while it waits, write AT
without waiting for the read deadline, receive OK on the concurrent reader, and
restore timeouts. Writer must finish in under 1000ms, configuration query in
under 500ms, reader must receive OK in 150-1500ms after a 200ms writer delay.
The expected result is 4/4 commands, 44 controls, zero failures, COMPLETE.

No WaitCommEvent, modem-status interrupt monitoring, purge/queue support or PPP
has been added. Full-duplex here means simultaneous synchronous RX/TX bulk
operations, not a complete CE serial/MDD or production modem implementation.

## Real-device results, 2026-10-11

Installed DLL SHA256:
`badae88bbd940314fbf7384c109483732d1d3b4e4582e3ecfb5461ad10947440`.

First test start tick13845940 preceded the first V7 attachment tick13872517
and READY tick13877909; it exercised the still-active V6 instance. Commands
4/4 passed but six concurrency checks failed; configuration queries took
1815ms, writes 2008-2010ms. Preserve test-first-attempt.txt and
driver-first-attempt.txt; do not attribute these failures to V7.

Second test start tick13933581: commands 4/4, checks 44, failures 0, COMPLETE.
COM1: config 12ms, write 4ms, reader 219ms/6 bytes/OK.
COM5: config 12ms, write 3ms, reader 219ms/6 bytes/OK.
See test-second-attempt.txt and driver-second-attempt.txt. Both old tests and
current logs remain available; published ATI output redacts the IMEI.

Third test, with deployment guards enabled, start tick14020260: 4/4 commands,
44 checks, zero failures, COMPLETE. COM1 config/write/read 12/2/216ms;
COM5 12/2/218ms, both concurrent reads returned 6-byte OK. The test waited for
port availability before opening; sampled power state was backlightoff, not
evidence of suspend or module power quality. Subsequent DETACH still occurred.
See test.txt and driver.txt. No long-term stability claim is made.

Microsoft's [CE5 CreateFile documentation](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa517318(v=msdn.10))
permits pending concurrent device reads/writes despite not supporting the desktop
FILE_FLAG_OVERLAPPED mechanism. No desktop OVERLAPPED workaround was introduced.
