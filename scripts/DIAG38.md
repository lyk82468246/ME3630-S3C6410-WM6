# D38: serial/USB flight evidence, not a modem-status compatibility fix

Inherits D37/D36 policies; private build38, public version18. Distinct
ME3630ComV18D38.dll, mapping ME3630D38Lifecycle. Exact19d2:1432 USB match
only; no force-unload, reset, saved-entry change, LeadCore or WMDC changes.
Automatic ADB/text logging remains disabled. Cancellation is still synchronous
and cleanup retains earlier limitations. The DLL installed705688 has SHA256:
11FB62DB92B9651ED8DE953586796368013A88BACE9A0B3118DF969988C9FB3F

## Trace semantics

Append-only event IDs41..60, same256-slot map schema as D37; distinct namespace
prevents mixing histories. Object pointers are opaque within one lifetime,
may be reused, and are not dereferenced after Close/release. Correlate by tick,
thread, attach generation and Open/Close; not just pointer equality.

| Event | object | value | error field |
| --- | --- | --- | --- |
| STATUS | transfer | GetTransferStatus BOOL | captured API LastError |
| USB_RESULT | transfer | bytes transferred | USB completion error |
| USB_RESULT_UNAVAILABLE | transfer |0 (no claimed output) | API LastError |
| OPEN_BEGIN | port | access flags | share flags, NOT an error |
| OPEN_END | port | returned Open context,0 failure | failure LastError |
| CLOSE_BEGIN/END | Open context |0 / BOOL | failure LastError |
| READ/WRITE_BEGIN | Open context | requested count | interface1/2 |
| READ/WRITE_END | Open context | returned count,FFFFFFFF failure | captured LastError |
| IOCTL_BEGIN/END | Open context | code / BOOL |0 / captured LastError |
| RX_PUSH | port | newly queued bytes | total queued bytes |
| RX_EVENTS | port | pending events | subscribed mask |
| MASK_STATE | port | new subscribed mask | retained pending events |
| WAIT_BEGIN/END | port |0 / BOOL |0 / captured LastError |
| WAIT_MASK | port | delivered event mask |0 |
| TRANSFER_ISSUE | port | requested bytes | interface OR100(IN)/200(OUT) |
| TRANSFER_SUBMITTED | port | transfer handle,0 failure | failure LastError |

USB_RESULT is emitted only if the status API returnedTRUE. Its error is not
Win32 LastError. API TRUE means a valid transfer structure, not an error-free
transfer: see Microsoft's CE5 [LPGET_TRANSFER_STATUS](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms902168(v=msdn.10)).
Issue is intent, not submission proof (e.g. TX purge can cancel before issue).
Read/write counts do not identify AT commands or PPP packets. No serial
payload, credentials, SIM identity or IMEI is copied to the recorder.

Wrappers preserve existing results/LastError/ownership and cover early failure
returns. WAIT wrapper delegates the existing implementation; no event-policy
change. Same-mask historical-event retention remains D36. GetCommModemStatus
is still unsupported; no fabricated CTS/DCD or unknown-success fallbacks.

## Build, tests, deployment

deploy_diag33.ps1 -Version38 builds D38 and separate life_*_d38 tools.
Joe32-bit RAPI native isolated tests: ownership15/15 at699914,
compatibility33/33 at702162, recorder19/19 at703355, all zero failures.
Four new production-helper checks cover event labels, API/USB separation,
actual USB error preservation, and unavailable outputs after API failure.
These are NOT full USB/removal/concurrency coverage; stream-wrapper behavior
is additionally observed below, not exhaustively unit-tested. D38 and legacy
D33 builds pass; all three PowerShell runners parse without errors.
Installer705688 result0 next_attach_only1. D38 actually attached757394,
COM1/5 activated762415/762439, READY2. Loaded naturally, no manual replug/reset
requested in this turn. Driver hash is unchanged after subsequent observer
rebuilds. The initial run_life revision-flag invocation failed during local
compilation before device launch; fixed by using the complete argument array.

run_life.ps1 -Version38 [-Watch|-Unit] collects the D38 namespace; D38 observer
uses its own remote file, not D37's file. Default remains D37. Saved-entry
runner -ExpectedDiagnostic38 checks public18 AND private38 before RasDial;
wrong/missing private identity aborts without dialing. It never changes the
entry or reads passwords and only hangs up its own returned connection.

## First actual dial:619, transport and event delivery advanced

First90s observer763658..853783 next1009/dropped0;941 records,60 explicit SKIP
(516 and519..577). Last emitted sequence1001; summary includes8 newer records
not drained by the old deadline loop. This is incomplete evidence, not a
lossless trace or proof that unobserved operations never happened.

Saved dial811637..822109,elapsed10472, private38/public18, exact own COM5
connection, RasDial0, final619/connected0; no PPP projection or IP success.
Repeated small writes succeeded with USB error0; responses queued, EV_RXCHAR
delivered, then Read returned6 bytes. E.g.812503 write4;812581 RX_PUSH6 and
WAIT_MASK1;812632 Read6. This is actual USB->queue->event->read completion,
not identification of response text. Read21 at817294 followed by Write37
817303..817308 on a different thread suggests a transition toward the data
path, but without payload framing no claim of PPP negotiation.

Handoff817295..817308 includes SetCommState FALSE/error50, metadata-open
SetMask/Wait FALSE/error5, data SetMask FALSE/error50, WAIT_MASK20 (EV_RLSD),
GetCommModemStatus001b0034 FALSE/error50, then data mask0. Correlate concurrent
IOCTLs by thread, not simply adjacent records. Metadata mutation rejection is
intentional ownership policy, not automatically the cause of619. No sampled
DSR/DCD direction/raw notification or DCB flags in D38; next revision needs
that evidence before a faithful modem-status compatibility design. Existing
CDC parser explicitly lacks CTS; do not fabricate it to return success.
Data/metadata Closed817749; physical USB detach825714 is later than RAS
failure/822109 completion, so it cannot directly explain the preceding619.
619 denotes ERROR_PORT_DISCONNECTED per local CE raserror.h and
[Microsoft's error list](https://learn.microsoft.com/zh-cn/windows/win32/rras/routing-and-remote-access-error-codes),
not proof of a particular failing component.

## Second dial and improved observer

To avoid slow per-record file flush while the ring is being overwritten,
only the independent D38 observer was changed: bounded batch snapshot before
formatting/writing, one flush per poll batch, final poll after the duration.
D37 observer behavior and the installed D38 DLL remain unchanged. Dropped/
SKIP/overwrite limitations still apply; no claim of guaranteed lossless capture.

Second saved dial995167..1000413 elapsed5246, private38, RasDial0, final633,
no connection/projection. At999405, status API TRUE but USB completion5
(USB_DEVICE_NOT_RESPONDING_ERROR from installed WM6 SDK usbtypes.h), Read
FFFFFFFF/error1117; both RX workers exited999406/999407. The subsequent9-byte
Write failed999408 with USB5/error1117. Both Open contexts closed999409,
USB_CLOSE_DEVICE999473, return intent999476, reattach1007380 and READY2
1012426. This633 has observed transport
failure before the outcome, unlike the first619 handoff failure. Do not label
all633 as the same lost-event/ownership bug or diagnose supply as sole cause.
Second observer973188..1063242: next711/dropped0, all711 records read, no
SKIP/overwrite. Another no-dial detach1049522..1049549 (27ms), attach1057172,
READY2 at1062218. The batching change improved completeness in this sample,
not proof of lossless recording under arbitrary rates. Mapping restarted its
sequence between observation windows after its last holders were released;
sequences are scoped to a mapping lifetime, not a persistent device history.

Focused evidence and final observer counts: evidence/2026-10-11-diag38.
Next: record exact rejected mask request/handoff DCB and actual carrier
status changes, decide faithful
standard modem-status policy, and distinguish response/PPP control metadata
without retaining raw serial payload. No blind asynchronous cancellation/free.
