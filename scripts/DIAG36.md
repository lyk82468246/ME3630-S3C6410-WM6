# D36: retain still-subscribed historical events across SetCommMask

Actual D35 private35 dial273324..283920 accepted the previously failing
timeouts/mask/purge. AT-size writes and six-byte responses were read. RAS
still returned633 without PPP. Last AT write279123 was followed by RX
push279142, then repeated mask1f9 at279178 and wait279231. D35 cleared
all pending events on every SetCommMask; no read followed before mask0/
close283071..283139. Detach287253 occurred afterwards. Timing + source
identify a lost historical RX event, not a claim of the only633 cause.
Focused trace and actual dial evidence are under diag35.

D36 uses shared production me3630_mask.inc under the control lock:
validate entire mask first, set it, advance cancellation epoch, retain
eventPending &= newMask, wake the waiter. It does NOT re-seed RX just
because unread bytes exist. Consumed events are not replayed, removed
subscriptions are filtered, zero clears, and existing waits still cancel.
All other D35 policies and CTS-unknown restrictions remain unchanged.

Primary comparison: Microsoft's serial driver sample SerialFinishOldWait
also intersects HistoryMask with the new mask rather than wiping it:
https://github.com/microsoft/Windows-driver-samples/blob/main/serial/serial/waitmask.c
This is a Windows sample reference, not proof of every CE5 OEM behavior.

Native isolated tests use production mask and wait helpers: the prior26
compatibility tests plus7 retain/deliver/no-replay/filter/zero/invalid/
same-mask cancellation checks,33/33 at411076. Ownership15/15 at408830.
No USB, AT or registry access during tests; not full concurrent USB proof.
Installed412364 result0 next_attach_only1 with independent DLL name,
only our existing exact19d2:1432 match updated. No extra device reset.
Version18/private36 required for actual attribution. DLL SHA256:
73DABE64855DDF070A807C55D73F95E0BAA03F0F22E816C922EBBA2AB691538F.

Build/install deploy_diag33.ps1 -Version36 in Joe32-bit; -BuildOnly
avoids device mutation. Preserve loaded DLLs; no forced unloading.
Original D35 remains available for comparison/rollback at next attachment.

Bounded post-install test430651..460871 found COM5 unavailable55 without
RasDial. User then replugged and waited30s; test535531..565803 again exited
after30272ms at unavailable55. Read-only Active ports0; accumulated trace
still ends287355, no new ATTACH. D36 real identity36 has NOT been observed,
so neither this fix's real effect nor633 resolution/PPP/internet is verified.
Evidence diag36/{dial-before-replug,dial-after-replug,state-summary}.txt.
No additional reset without user approval; recovery is required for next test.
