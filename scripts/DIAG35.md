# D35: measured Unimodem timeout, purge and event compatibility

D34R2 measured RAS timeout parameters 50/1/50/5/500, event mask 0x1f9
and purge 0x0c, all previously rejected. D35 implements these paths rather
than blindly acknowledging unknown controls. Per-open ownership remains D34.

## Semantics and limits

- Read total = multiplier * requested bytes + constant. Ordinary interval
  timeout starts after data arrival and resets on subsequent queued arrivals.
  Immediate and first-byte MAXDWORD modes are separate. Zero/zero disables
  total timeout; stop/abort still terminates waits. Finite configuration fields
  are bounded at 10000, and per-call budget arithmetic rejects overflow.
  Arrival timestamps are USB batch arrivals, not physical UART byte timing.
- Write uses the requested-count budget. TXCLEAR increments a generation
  under the control lock; pending chunks are checked before USB submission.
  Already submitted USB data cannot be retracted. RXCLEAR clears the host
  FIFO, not the modem's internal buffers. TXABORT remains atomically rejected.
- Mask 0x1f9 is accepted. RXCHAR/DSR/RLSD/RING retain existing behavior;
  valid CDC BREAK and frame/parity/overrun packets feed events and sticky
  ClearCommError bits. CTS subscription is passive: CTS remains unknown,
  no CTS state/edge is invented, and hardware CTS flow control is rejected.
  RXFLAG and TXEMPTY remain unsupported. This is not full serial conformance.
- DCB is unchanged: initial 19200/hardware-handshake configuration still
  rejects, while the previously measured 115200 fallback is supported.

## Verification and installation

Joe 32-bit RAPI deployment runs native tests before installing:
15 ownership checks at tick9635798 and 26 compatibility checks at9638099,
all zero failures. Compatibility tests use production read/timeout/purge/
notification helpers with an isolated fake port: real timing, no USB, AT,
SIM, registry changes or physical line-error injection. They do not test
actual in-flight USB TX-clear races, full detach concurrency or PPP.
The check named real_error_break uses a synthetic valid CDC packet, not
an observed hardware error.

Install tick9639413 result0, next_attach_only1. Only the existing exact
19d2:1432 ME3630Probe match changes to ME3630ComV18D35.dll. LeadCore,
WMDC, Unimodem and the user's saved connection are preserved. No force
unload or reset. Version remains18; private diagnostic identity must be35
before attributing real results to D35. DLL SHA256:
67678A0D7757B4105A950B43E9BBABDA302933CC9ECE5BD35283A8CFFD02613B.

Initial bounded test tick9851236..9881395 found COM5 unavailable error55;
Active ME3630 ports0, exact match D35. RasDial was not called, and D35
activation/633 resolution/PPP/internet are not verified by that test.
After user-confirmed replug and30s wait, second test9970118..10000290
again found error55 without calling RasDial. Accumulated driver log still
ends at1719938, with no new ATTACH. No unapproved reset was performed.
Evidence: evidence/2026-10-11-diag35/{open-unit,compat-unit,install,
dial-port-unavailable,dial-after-replug,state-summary}.txt. Full inventories remain local.

The saved-entry collector now tolerates transient IOException snapshots
within its existing deadline; it does not start additional remote processes
because a log read failed. One such retry was observed on the bounded test.
The legacy D33 branch also still compiles after these conditional changes.

Build/install: deploy_diag33.ps1 -Version35 (Joe32-bit); add -BuildOnly to
avoid device writes. Do not overwrite an active same-name DLL. Roll back
by restoring only our exact match to the preserved prior D34R2 DLL and
allowing natural next attachment; registry changes do not unload active code.

## Primary references

Later user-approved WM6 reset allowed actual private35 test273324..283920.
Measured timeout/mask/purge now succeed and AT-size transfers get responses,
but RAS still633, no PPP. See diag35/dial-active633.txt and
driver-event-race.txt plus DIAG36.md for the newly identified pending-event
loss and next isolated fix. The earlier absent-port results remain preserved.

- CE COMMTIMEOUTS: https://learn.microsoft.com/en-us/previous-versions/ms938944(v=msdn.10)
- CE SetCommMask: https://learn.microsoft.com/en-us/previous-versions/ms912823(v=msdn.10)
- PurgeComm: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-purgecomm
- Official Linux CDC UART state constants (bits0..6, no CTS):
  https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/cdc.h
