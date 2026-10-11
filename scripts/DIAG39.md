# D39: exact configuration requests and genuine carrier observations

Diagnostic only, inherits D38/D36 ownership, serial/event policy, synchronous
cancellation and cleanup. It does NOT implement GetCommModemStatus, TXEMPTY,
hardware flow control or a hotplug recovery fix. Public18/private39, distinct
ME3630ComV18D39.dll and ME3630D39Lifecycle mapping. SHA256 installed9678003:
E632C4A9FA44A43B387118332C663BD72DE7978ACDAD820C1B730F73EE28C37D

## Added event IDs61..70

Same256-slot schema; prior IDs unchanged. Independent observer batches snapshots
before file writes. Absence of records is not proof absence of a USB callback:
different/older driver, map initialization/loading failure and unobserved host
activity remain possible. Recorder is not guaranteed lossless.

| Event | object | value | error field (not necessarily an error) |
| --- | --- | --- | --- |
| REQUEST_SIZE | Open context | input bytes | output bytes |
| REQUEST_WORD | Open context | SetMask/Purge IOCTL | exact requested DWORD |
| DCB_LENGTH_BAUD | Open context | DCBlength | BaudRate |
| DCB_FLAGS_FORMAT | Open context | raw DCB flags DWORD at SDK offset8 | ByteSize OR Parity<<8 OR StopBits<<16 |
| TIMEOUT_INTERVAL | Open context | read interval |0 |
| TIMEOUT_READ | Open context | multiplier | constant |
| TIMEOUT_WRITE | Open context | multiplier | constant |
| LINE_STATE | port | stored modem status | validity mask |
| LINE_SAMPLE | port | parsed raw signal flags | sample age ms, FFFFFFFF if invalid/unknown |
| LINE_EVENTS | port | subscription mask | pending events |

Request records run before metadata ownership rejection, so rejected requests
are captured too. Correlate each group to IOCTL_BEGIN using thread and Open
context; concurrent groups may interleave. Only exact-sized known configuration
inputs are copied, never raw serial traffic, unknown IOCTL payload, passwords,
SIM identity or IMEI. Null and wrong sizes are not dereferenced. Like the
existing control path, valid-sized non-null pointers rely on Device Manager
buffer mapping; this is not arbitrary-address validation.

Line triplets are captured under the existing port lock after valid strict
notifications, after notification-worker invalidation, and on data-owner
GetCommModemStatus/private-state requests. Metadata-denied controls do not
enter that locked path. First valid notification establishes a baseline, not
an edge. Subsequent RLSD status permits distinguishing rise/fall if enough
records survive. CTS is never marked known. Zero known means stale/unknown,
NOT all lines low. LINE_SAMPLE invalid sentinel does not encode the cause;
cross-check transport/exit records. This does not change the API result.

## Validation and first deployment

deploy_diag33.ps1 -Version39 builds inherited feature flags and unique D39 tools.
Joe32-bit native isolated tests: ownership15/15 tick9672278, compatibility33/33
tick9674520, recorder/request/line29/29 tick9675717, all zero failures (77 total).
Ten new checks cover exact unsupported-mask encoding, null/short/unknown
inputs, original DCB preservation and format/baud/flags, timeouts, line validity
and LastError preservation. These do not validate real USB cancellation races
or prove successful dialing. All PowerShell scripts parse without errors.

Install9678003 result0 next_attach_only1: only our exact existing19d2:1432 DLL
match changed. No LeadCore/WMDC/saved-entry changes or forced unload.
run_rassaved ExpectedDiagnostic39 requires actual public18/private39 before
RasDial, preserving the existing exact COM5/China Telecom entry and number.

First observer9700364..9790452 elapsed90088 next0/dropped0. First guarded
attempt9713734..9744220 elapsed30486 PORT_UNAVAILABLE55; no RasDial.
Read-only Active snapshot0; HCD1 remains present, exact match names D39.
User then confirmed one replug plus30s. Second observer9806079..9896081
elapsed90002 next0/dropped0; second attempt9819013..9849166 elapsed30153
also PORT_UNAVAILABLE55, no RasDial. These are failed enumeration/loading
windows, not a new D39 serial compatibility or PPP result. No exact handoff
mask/DCB/carrier conclusion can yet be drawn from these empty observations.

User subsequently authorized ONE WM6 soft reset. Request PID3437227770;
first post-reset RAPI Connect failed0x8000ffff. Reconnection is not a second
reset. Subsequent observer54517 and saved-attempt73066 exceeded host collection
windows with incomplete local results. Neither is a complete RAS/driver result;
the saved-attempt result was re-read before another test and still incomplete.
Do not infer a permanent driver/observer hang or a specific RAS error.

## Post-reset evidence and the collector defect

Read-only Active snapshot showed two own ports. Independent one-shot readers
then proved D39 can run: snapshot257316 next115/dropped0, READY2 at231598;
later fully retained snapshot415069..415160 next159/dropped0, ATTACH374619,
READY2 at379664. At380248 both ports had known=e0, status0, raw0, age0.
This is a genuine valid low-carrier sample, not fabricated CTS, not cellular
registration information and not a carrier rise/fall at a PPP handoff. There
were no Open/request records in that retained snapshot. Map sequences restarted
between lifetimes; incomplete observations cannot count all detach cycles.

One exploratory OpenProcess/GetExitCodeProcess query of historical PIDs returned
noncanonical BOOL values and code0 without held original handles; no conclusion
is based on it. Final snapshot code instead uses Toolhelp, showing only its own
process among ME3630 names at415069 and661617. Crash-dump directory count0 is
not proof that no exception happened. No process was terminated by us.

New supervisor holds a recorder mapping and REAL child process handles. Test
555460..645547 elapsed90087: both child creation succeeded; actual wait0 and
exit_query1/exit_code0 for capture732995454 and dial1270102386. Re-reading
closed files recovered full results: capture555490..645507 next0/dropped0;
saved attempt555533..585664 elapsed30131, port unavailable55, no RasDial.
This later no-module sample is conclusively normal process completion, not a
hung observer. It does not retrospectively prove the initial incomplete tests
exited normally. Exact handoff parameters remain unmeasured; no PPP/IP.

RAPI helper can return false AFTER truncating/writing its local destination.
Original polling consequently could leave an incomplete local file instead of
the last successfully copied snapshot. New rapi_snapshot.ps1 copies to a staging
file, publishes only on true, and leaves previous evidence unchanged on false
or exception. A successful copy is still only a snapshot; freshness and COMPLETE
checks remain mandatory. Supervisor re-reads both CLOSED child files and validates
start ticks/completions/exit codes. No claim of lossless live collection.
Five host-only fake-RAPI tests passed, including partial-false, partial-exception
and recovery. D39 recorder29 tests re-ran on actual WM6 at911177 with zero failures
using the new snapshot helper. All PowerShell scripts parse. Installed driver
hash remains unchanged; this is a collector fix, not a new DLL policy.

Relevant CE API contract: [GetCommModemStatus](https://learn.microsoft.com/en-us/previous-versions/ms928824(v=msdn.10))
returns current signal bits, without a separate validity mask. Unknown CTS must
not silently be asserted high to pass a test. [SetCommMask](https://learn.microsoft.com/en-us/previous-versions/ms912823(v=msdn.10))
describes RXFLAG/TXEMPTY as actual receive/transmit events, not arbitrary success
fallbacks. Actual caller parameters and transport/carrier evidence are required
before selecting compatible implementations.

Focused evidence: snapshot-active, snapshot (later empty map), supervised/*-final,
the two pre-reset empty captures, fresh isolated tests and install. Full registry
snapshots and failed-copy staging files stay local. No second reset was issued.

Rollback: point only the same ME3630 match back to D38 using an explicitly
scoped installer; do not overwrite a loaded DLL, alter other matches, reset
USB Host blindly or infer another reset authorization from this document.
