# D37: low-impact lifecycle flight recorder, not an automatic-recovery fix

This stage addresses observability before changing CE USB handle lifetimes.
It inherits D36 serial/event policies and identifies as version18/private37.
Only the exact existing19d2:1432 match is installed; LeadCore, WMDC and the
saved connection are preserved. No reset or force unload is part of deployment.

## Changes

- Driver logline is a no-op in D37. No file append/traceLock acquisition in
  transfer/cleanup callbacks. Older versions retain their existing logging.
- Automatic ADB boot probing is disabled only in D37. No AT/RAS command is
  automatically initiated; DTR/RTS startup initialization remains as before.
- Named page-backed memory ring contains256 fixed records, sequence/tick/
  process/thread/event/object/result/error only: no serial payload, credentials,
  IMEI or memory dumps. A writer uses try-CAS per slot, never waits; contention
  drops are counted. Readers validate commitment around copying and reject
  busy/overwritten records. 32-bit positive sequence wrap is not supported.
  `dropped` counts contention/stale writes, not all ring overwrites.
- Begin/end milestones surround device-info lookup, registration/removal,
  worker joins, DeactivateDevice, direction/control lock acquisitions,
  ClosePipe, resource release and synchronous AbortTransfer/status/CloseTransfer.
  Abort failure and API results are recorded. USB cancellation semantics are
  deliberately NOT changed yet; zero-flag abort may still block indefinitely.
- DETACH_RETURN_INTENT is before returning from the callback, not proof of
  host resumption. CLEANUP_END means the loop reached its end, not proof every
  API succeeded. RETAIN branches and individual results must be checked.
  READY value counts actual successful ActivateDevice calls, not RF readiness.
- Object fields are opaque correlation keys; release-END never dereferences
  freed objects. The recording map closes on DLL unload. An independent
  observer must hold it open to preserve records across unload/reattach.

## Observation and limitations

run_life.ps1 -Watch launches a90s observer before a user replug, holds the
named mapping and collects fresh start/COMPLETE within110s. Default is an
immediate snapshot. -Unit uses a distinct per-process map and does not touch
USB/COM/registry. Only the observer writes its own log; callbacks don't.
Observer output reports overwrites/SKIP/dropped, so missing events in an
incomplete ring are not by themselves proof of a hang. An unpublished record
can be skipped by observation; no claim of lossless instrumentation.

No driver mapping/events may mean D37 hasn't loaded, a previous driver/host
cleanup is stuck, driver loading failed or recorder initialization failed.
It does NOT prove the USB port electrically failed or the old callback wasn't
entered. This instrumentation cannot retroactively diagnose old D35 cleanup.
No blind HCD reset/unload, async buffer release or replay of stale USB handles.

## Verification

D37 native build and legacy D33 build succeed. On WM6 Joe32-bit RAPI:
ownership15/15 at1278399; compatibility33/33 at1280649;
production flight-recorder15/15 at1281818, repeated15/15 at1381349.
Checks include error preservation, publish validation, busy writer drops,
ring wrap/overwrite rejection, concurrent writers, actual child-process
named-map sharing, and retained-map lifetime. These are isolated native
tests, NOT actual USB cancellation, teardown or re-enumeration validation.

Install1284123 result0 next_attach_only1. D37 DLL SHA256:
93C12ED850AEF4F9DA76AA19F16ED4136BCA4F58E7015ADA6654621B82CE7485.
build/ME3630ComV18D37.dll is the installed build; observer was subsequently
rebuilt separately for90s observation, without rebuilding the driver.
Current actual observation outcome is recorded in evidence/2026-10-11-diag37
and EXPLORATION_LOG.md; installation is not activation evidence.

## First actual post-reset observation

The user explicitly authorized one soft reset, launched as PID3431585634.
The first RAPI connection timed out; the second connected without another
reset. The pre-reset user-replug observation1385829..1475988 had next0.
The fresh90s observation46668..136751 had211 records, dropped0 and no SKIP
or overwrite indications. ATTACH70612 registered successfully; COM1/5
ActivateDevice succeeded75643/75674 and READY counted2.

RX workers exited112370 with1117; USB_CLOSE_DEVICE entered112401.
Worker and both notify/RX joins returned0, unregister and both deactivations
returnedTRUE. All recorded locks and resource-release stages completed;
DETACH_RETURN_INTENT112455 was54ms after entry. All six ClosePipe calls
returnedFALSE/GetLastError6. This is an observed failure, not proof of a
double-close, leak, or the earlier loss of enumeration. A transfer API
FALSE with recorded last-error0 is still failure; that field is not the
USB completion status returned through GetTransferStatus's output pointer.
The latter USB status and byte count are not currently recorded by D37.

A new ATTACH120113 (7658ms after return intent), successful activations
125134/125157 and READY2 demonstrate actual subsequent enumeration in this
sample. This excludes a permanent cleanup blockage in this specific cycle,
not all historical/hypothetical hangs. No A/B trial separates logging removal
from disabled automatic ADB probing or the reset itself. No dialing/PPP test
was performed. Read-only Active0 snapshots straddled enumeration/teardown;
they do not negate the timestamped activation evidence.

ClosePipe can itself wait for pending transfers; retain the earlier lifecycle
audit's risk even though this cycle returned quickly. Primary reference:
https://learn.microsoft.com/en-us/previous-versions/ms887621(v=msdn.10)

Follow-up observer158618..248822 completed with next579/dropped0. It replayed
retained history and reported5 older overwritten records at startup; those
first5 are preserved in capture-reset-baseline.txt. No SKIP indications.
Two additional observed cycles: detach160945..161004 (59ms), attach168628,
READY2 at173676; detach209873..209876 (3ms), attach217557, READY2 at222605.
Across three distinct detach cycles, six deactivations succeeded and18 pipe
closes returnedFALSE/error6. No join timeout/RETAIN was observed in these
cycles. The user confirmed no replug or power changes after the single reset.
Thus automatic re-enumeration is now demonstrated for this limited no-dial
sample, not continuous stability or successful PPP. Replayed history is not
counted as an extra detach.

Next diagnostics should expose USB completion status/bytes and bounded
serial Open/Read/Write/IOCTL milestones in memory before a PPP trial; D37's
disabled text logging alone is insufficient to locate a new RAS failure.
Investigate pipe ownership on removal before adding blind retries or treating
error6 as benign. Do not free buffers after asynchronous cancellation without
a proven retirement protocol; synchronous cancellation remains unchanged.

Build/install: deploy_diag33.ps1 -Version37 (Joe32-bit); -BuildOnly avoids
device writes. Independent tests/observation: run_life.ps1 [-Unit|-Watch].
Preserve an active DLL and use a distinct revision filename for future changes.

Primary references:
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa517331(v=msdn.10)
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms933771(v=msdn.10)
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms902160(v=msdn.10)
