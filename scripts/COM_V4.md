# CE5.2 COM V4 prototype

## V5 follow-up

Use build_com.ps1 -Version5 / deploy_com.ps1 -Version5 for the 5-second-startup
variant, ME3630ComV5.dll. It retains the same bounded serial subset and COM1/COM5
mapping. V4's 20-second delay often outlasted an observed enumeration window.
V5 is a timing workaround, not a fix for repeated disconnects. Existing V3/V4
files remain available; V5 installer falls back to V4 registration on failure.
Some inherited driver log lines still say V4 ACTIVATE; DLL name, V5 WAIT and V5
READY identify the installed version.

Latest V5 real-device result: 4/4 AT/ATI, 32/32 control checks, no failures.
Empty reads measured 352ms for a 350ms setting. One-byte reads preserved the
remaining 5-byte AT and 111-byte ATI responses on both ports. Evidence lives in
evidence/2026-10-11-com-v5/test.txt. Re-enumeration still occurs after the test.

V5's native test waits for ports within a shared 40-second deadline, samples
system/host power state read-only, and starts every command response with a
one-byte read to verify retained receive data. Tests use unique EXE filenames.
build_com.ps1 -Version5 -TestsOnly updates the test without rebuilding a loaded
DLL. Deployment reads results after 50 seconds; absence of a COMPLETE marker is
not a passing result. This ROM returns error55 for a rejected second open; the
test accepts 55 or 32 and subsequently proves the original handle still works.

In V4's observed disconnect cycles, GetSystemPowerState reported system=on,
flags=0x12010000. GetDevicePower(HCD1:) returned error2, so host power state was
unavailable, not evidence of D0 or suspend. SystemIdleTimerReset did not reliably
prevent re-enumeration. Power/cable/firmware/host hypotheses remain unresolved.

ME3630ComV4.dll exposes MI1 as COM1: and MI2 as COM5: (these names were free in
the 2026-10-11 device snapshot). Device Manager activation must succeed; never
remove another driver's active port to resolve a collision. USB matching remains
19d2:1432 only. Existing LeadCore and WMDC drivers are unchanged.

## Measured ABI

The WM6 SDK lacks Pegdser.h. A temporary TRC1: stream driver received the exact
DeviceIoControl requests sent by native serial APIs on this ROM (CE5.2 build20782).
It never accesses USB or existing COM ports, and deregisters on exit. The trace
driver intentionally returns ERROR_NOT_SUPPORTED; those failures are expected.
Evidence: evidence/2026-10-11-com-v4/ioctl-abi.txt.

| Native API | Measured code | Buffer |
| --- | --- | --- |
| GetCommState / SetCommState | 0x001b0050 / 0x001b0054 | DCB, 28 bytes |
| GetCommTimeouts / SetCommTimeouts | 0x001b0040 / 0x001b003c | COMMTIMEOUTS, 20 bytes |
| SETDTR / CLRDTR | 0x001b000c / 0x001b0010 | none |
| SETRTS / CLRRTS | 0x001b0014 / 0x001b0018 | none |

Other measured requests are recorded in the trace but deliberately unsupported
in V4. These are this ROM's measured ABI values, not a claim of all CE versions.

## Supported subset

- Get/SetCommState: 115200, 8N1, binary, no software/hardware flow control.
  Only enabled/disabled DTR/RTS; requests use the previously verified USB control
  request. Baud settings are logical; no UART line-coding request is implemented.
  Other settings fail explicitly instead of being silently ignored.
- Get/SetCommTimeouts: total-only reads (interval and multiplier zero), or the
  first-byte special mode (interval and multiplier both MAXDWORD). Read constant
  1..10000ms. Write multiplier zero, write constant 1..10000ms. Unbounded,
  immediate/poll and inter-byte configurations are explicitly unsupported.
- Read timeout succeeds with the accumulated count, including zero bytes.
  A 512-byte receive staging buffer retains bytes not consumed by a smaller read.
  Bulk requests always use the full receive staging size rather than asking USB
  to deliver a short application buffer. Writes loop over 512-byte chunks.
- One opener per port. Cancellation on removal and reference-counted contexts
  are inherited from V3. An I/O lock serializes read/write/control: NOT full-duplex.
- Startup resets the system idle timer for 20 seconds only; tests keep the device
  awake temporarily. No persistent power policy is changed.

## Limitations and verification

No modem status/interrupt monitoring, WaitCommEvent, comm properties, purge,
queue sizing, break, full-duplex buffering or functional suspend/resume handling.
PowerUp/PowerDown remain stubs. Do not use V4 for Unimodem/PPP yet. Long reads can
delay control/write calls; actual unplug-with-open-handle behavior still needs
testing. A successful DTR/RTS request does not prove the electrical line state.

build_com.ps1 creates new V4 filenames without rebuilding/replacing V3 binaries.
deploy_com.ps1 installs via actual Joe context + 32-bit PowerShell, retaining
V3 files and attempting V3 registration fallback if V4 registration fails.
Already loaded clients change only after a module replug. -Test checks DCB and
timeout roundtrips, unsupported settings, four control-line requests, the
empty-read deadline, exclusive opening, and AT/ATI on each port. The required
summary is COMMANDS_PASSED=4/4 CONTROL_CHECKS=32 FAILURES=0 COMPLETE. IMEI is
redacted in published evidence; raw logs remain ignored locally.

Sources consulted for API semantics (actual CE codes were independently measured):

- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms901434(v=msdn.10)
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts

The latter is Win32 timeout documentation; V4 supports only the explicitly tested
subset and must not be described as full CE serial/MDD compatibility.
