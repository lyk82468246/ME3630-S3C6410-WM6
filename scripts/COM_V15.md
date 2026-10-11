# V15 queue status and receive purge subset

V15 inherits the V14 5000ms receive-pump deadline, driver-owned TX staging,
ring and independent event/read waits. It has separate version15 DLL, installer,
registry, log and test names, private instance query15 and V14 fallback.
No dial, RF/APN/SIM, modem reset, power-policy or unrelated driver changes.

## API scope

ClearCommError uses the measured CE5.2 IOCTL001b0030 with16-byte output:
DWORD errors plus12-byte COMSTAT. cbInQue reports the protected local ring
count; cbOutQue tracks unsent bytes of the current synchronous write, updated
under the configuration lock. This is not a hardware/module queue snapshot.
Hold flags are zero because the supported DCB disables all flow-control holds,
not because physical CTS/DSR/DCD is asserted. No framing/parity/break detection
exists. Fatal RX failure is reported as CE_IOE; acknowledging status does not
repair a broken USB connection or clear the fatal stop/failure condition.

PurgeComm uses measured IOCTL001b0044, DWORD input. Only PURGE_RXCLEAR,
PURGE_RXABORT, or their combination is supported. RXCLEAR empties the local
ring and removes its pending RXCHAR notification without changing the event
mask/generation or cancelling WaitCommEvent. It does NOT flush the module or
abort the ongoing USB receive. A transfer completing after the clear may
enqueue bytes, even if they originated earlier; this limitation is explicit.

RXABORT increments a separate reader generation and wakes the reader. An
already entered synchronous queue read detects the new generation, returns
ERROR_OPERATION_ABORTED if no bytes were copied, or a successful partial read
if it had already copied bytes. Subsequent reads use the new generation and
can receive normally. Reader serialization and configuration-lock order are
preserved. No USB cancellation or arbitrary thread termination is used.
This is the prototype's synchronous-read extension, NOT overlapped I/O support.
Multi-reader cancellation, partial-read cancellation and all purge/arrival
races are not exhaustively validated by the single pending-reader USB suite.
The isolated WM6 test below covers two serialized readers without real USB.

The entire purge mask is checked before any queue mutation: TXCLEAR/TXABORT
and unknown flags return ERROR_NOT_SUPPORTED without clearing anything; zero
is rejected as ERROR_INVALID_PARAMETER. No pretend successful TX purge.

## Native verification

status_test.inc adds18 checks per port to V14's102, target138 plus AT/ATI4/4.
It verifies COMSTAT size, six-byte reply count, one-byte consumption leaving
five, mixed unsupported/zero purge preserving those five, RXCLEAR removing
them, a200ms empty read, prompt RXABORT/error995 and subsequent complete AT.
Reported writer-pending count is implemented but not yet stress-tested during
an active bulk write. The suite does not validate modem-line status or PPP.
Actual hardware results are recorded below after completion.

Primary API references:
- https://learn.microsoft.com/en-us/previous-versions/ms912766(v=msdn.10)
- https://learn.microsoft.com/en-us/previous-versions/ms907277(v=msdn.10)
- https://learn.microsoft.com/en-us/previous-versions/ms938946(v=msdn.10)

ABI evidence: evidence/2026-10-11-com-v4/ioctl-abi.txt. Full/raw inventories and
ATI identifiers remain local; focused test output is redacted before publishing.

## Actual results

Installed DLL SHA256:
`b3cc7086a4411f3ebf2ed914f72a670a2706a568140ed21b699963f0da90c6b5`.
Installation dll_load1 installed1 error0. First native run tick1342462 verifies
instance15 on both ports, AT/ATI4/4, CHECKS138 FAILURES0 COMPLETE. Queue
counts6 then5 then0, rejection atomicity, read timeout and post-abort AT pass.
Purge calls15ms; reader lifetimes198/197ms include startup delay and logging,
not pure cancellation latency. Both readers report error995 with zero bytes.

Second run tick1470145: AT/ATI4/4, CHECKS138 FAILURES20 COMPLETE. COM1 tests
pass; COM5 loses USB during backlog at RX_EXIT1477405 /DETACH1477458, and
subsequent purge/status operations return disconnect errors. It is not a
complete second pass. First/second focused traces and tests are preserved;
module reboot remains unresolved rather than hidden by automatically reopening
failed test handles. A bounded third hardware test is recorded on completion.

queue_unit.c/run_queueunit.ps1 -Version15 runs production ring/pump/read with
fake transport, no USB or serial I/O. Separate queue_unit_v15.exe and logs
preserve the earlier unit version. Tick1514311: CHECKS13 FAILURES0 COMPLETE,
including abort releasing the active reader AND a second reader serialized
behind rxIO, plus a new-generation reader receiving all six simulated bytes.
This validates queue cancellation in one process, not stream-manager/USB races.

Third fresh hardware run tick1537677 verifies instance15 on both ports, AT/ATI4/4,
CHECKS138 FAILURES0 COMPLETE; purge calls16/14ms and readers report error995,
then normal AT recovery. Two complete passing windows with a disconnect-
interrupted failure between them, NOT consecutive success or long-term stability.
