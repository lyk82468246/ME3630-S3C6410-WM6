# V8 RXCHAR event subset

V8 inherits V7 concurrent synchronous RX/TX, V6 read-only boot diagnostics and
the same exact USB/COM mappings. It is not a complete modem/PPP driver. Existing
V7 remains the installer's fallback. No network/RF/SIM or WMDC/LeadCore changes.

This ROM's native API ABI was measured in ioctl-abi.txt:

| API | IOCTL | Buffer |
| --- | --- | --- |
| GetCommMask | 0x001b0024 | DWORD output |
| SetCommMask | 0x001b0028 | DWORD input |
| WaitCommEvent | 0x001b002c | DWORD output |

Supported masks: zero and EV_RXCHAR only. Unsupported bits fail with
ERROR_NOT_SUPPORTED without changing the mask. A mask generation counter lets
SetCommMask cancel an existing waiter, including setting the same mask again.
Cancellation succeeds with event zero; disconnect fails rather than generating
a fictitious RX event. Only one waiter per port is permitted (ERROR_BUSY).

The waiter performs bounded 200ms USB bulk receive attempts and owns the RX
lock. New bytes remain in the existing 512-byte staging buffer for ReadFile;
TX/config are not blocked. The public wait itself has no artificial timeout:
it ends on RX, mask change or disconnect. Explicit event-wait references retain
the port context. Ordinary reads also record new RX events under configuration
locking. This uses our measured CE interface, not desktop OVERLAPPED handling.

Limitations: this is a wait-then-read workflow, not an independent receive pump.
Do not run a simultaneous ReadFile and WaitCommEvent on the same port. Drain
the buffered response before waiting again; a second waiter does not fabricate
another event for the same unread bytes. Calling another wait with unread data
can hold RX until mask cancellation/removal, blocking a competing reader.
Mask cancellation latency is normally bounded by the 200ms USB slice, but an
already active ReadFile holding RX may delay the waiter entering its loop.
These limitations must be removed before general Unimodem integration.

GetCommModemStatus, CTS/DSR/DCD/RING events, EV_TXEMPTY, line-error reporting,
purge and queue sizing remain explicitly unsupported. No status bits are faked.
The module interrupt endpoints have not yet been decoded as trusted modem
status. Presence of a USB endpoint alone is not proof of CDC SERIAL_STATE.

Private diagnostic IOCTL 0x00223ffc returns DWORD version=8; this is our own
interface, not a standard serial API. V8's native test queries it on each opened
handle and retries a wrong/old instance within the 40-second availability window.
This is stronger than a historical READY marker. Host-side deployment also
requires a fresh start_tick and rejects old COMPLETE output.

Build: build_com.ps1 -Version8. Deploy/test: deploy_com.ps1 -Version8 and
-Version8 -Test via Joe's actual 32-bit WMDC PowerShell. Publish redacted logs;
keep test-raw.txt local. Expected test result: commands 4/4, controls 68,
failures 0, COMPLETE (44 previous checks +12 event checks per port).
The stronger retest combines waiter creation with rejection of a duplicate
WaitCommEvent, and mask readback with a zero-mask immediate wait; count stays 68.
Test logs explicitly record INSTANCE_VERSION=8 per opened port.

Installed DLL SHA256:
`a51b4bc2d42e8df87ef71833224a8bd1a3dfcf06fd208af79761cc267f2ef600`.

First real-device round, start tick14309370: commands 4/4, checks 68,
failures 0, COMPLETE. RX event latency including the test's 200ms writer delay:
COM1 212ms, COM5 206ms. Both events reported EV_RXCHAR and retained 6-byte OK
responses. Cancellation: SetCommMask 12/17ms, total wait lifetime 215/232ms;
this lifetime includes the preceding 200ms delay, not just cancellation latency.
Published first-round evidence: test-first-attempt.txt, driver-first-attempt.txt.
Subsequent module DETACH still occurred; no root-cause/stability claim is made.

Stronger second round, start tick14414419: INSTANCE_VERSION=8 on both handles,
commands 4/4, checks 68, failures 0, COMPLETE. Duplicate waits were rejected with
ERROR_BUSY=170 and zero event output; zero-mask waits returned immediately.
COM1/COM5 RX waits 206/207ms, AT data retained; cancel calls 11/15ms, total wait
213/219ms. The same installed DLL was used, only the test EXE was rebuilt.
Evidence: test.txt and driver.txt. Legacy V6/V7 conditional builds also passed
compiler syntax checking without replacing their preserved binaries.

Primary API reference: [Microsoft CE WaitCommEvent](https://learn.microsoft.com/en-us/previous-versions/ms913508(v=msdn.10)).
This documents the CE event interface; our implemented subset and cancellation
behavior must be judged by local tests, not inferred from all listed events.
