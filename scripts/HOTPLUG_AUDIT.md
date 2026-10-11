# 2026-10-11 read-only audit: why repeated WM6 resets are not acceptable recovery

Scope: source and existing evidence only. No reset, install, driver/source
implementation change or new hardware test. This is diagnosis, not a proven
identification of the lost-enumeration root cause.

## Three separate failures

1. Earlier /proc uptime + boot_id changes establish actual module Linux
   reboots across attachments (EXPLORATION_LOG, V6 and V16 sections). They
   do not identify power, watchdog, firmware or driver-triggered USB failure.
2. D35 RAS633 preceded its final DETACH by several seconds. Mask-reset
   event loss is a measured software defect, addressed by D36 in isolation;
   neither explains all module reboots nor proves lost enumeration fixed.
3. After latest DETACH, manual replug produced no new logged ATTACH and
   Active ports0; WM6 reset previously restored attachments. This implicates
   a wider lifecycle/USB-host recovery boundary, not simply a stuck app COM
   handle. Missing ATTACH is not proof our callback was never entered: its
   first trace follows lpGetDeviceInfo, and driver loading/logging can fail.

## Confirmed architectural gaps, with source references

- me3630_probe.c:204 detach runs synchronous worker join (up to10s), self
  notification removal, stream_cleanup and parent release inside the USB
  close callback. stream_cleanup joins up to5s each per RX/notify worker
  and calls DeactivateDevice, acquires direction/control locks and closes
  pipes. These operations are not covered by one bounded callback deadline.
  Actual OEM host-lock/callback deadlock has NOT been established.
- me3630_probe.c:126, me3630_serial.inc:serial_transfer and
  me3630_notify.inc use lpAbortTransfer(t,0). Microsoft CE5 states that
  zero flags block until cancellation; an outer polling timeout therefore
  does not bound the cancellation call. Return values of cancellation,
  transfer status/close and pipe close are not fully checked. Blindly using
  USB_NO_WAIT then freeing/closing live buffers would create a lifetime bug,
  not a safe fix.
- stream_cleanup:492 does not check/log DeactivateDevice result; pipe close,
  port release and detach callback return have no completion milestones.
  DETACH wait0 is logged BEFORE cleanup; RX/notify JOIN wait0 is likewise
  BEFORE DeactivateDevice and pipe closure for that port. Prior wording
  treating these as full cleanup confirmation was too strong.
- Latest trace stops at287355 second RX_JOIN. This is consistent with an
  unobserved later cleanup stall, but also with successful unlogged cleanup.
  Active0 doesn't prove the entire callback returned. Earlier cycles did
  reattach after join0, so failure is not universal.
- On timeout, RETAIN_STUCK branches retain base references/pipes and skip
  cleanup, with no retirement/reaper mechanism. That protects against UAF
  but is indefinite quarantine, not complete recovery. No such timeout was
  observed in the latest focused trace; it is a code-path risk, not measured
  evidence of an actual leak here.
- RX/notify transport failure can terminate a worker while its COM port
  remains registered; no in-generation endpoint retry/restart state machine.
  A fresh ATTACH builds fresh ports, but application retry cannot revive a
  stopped receive pump or produce a USB enumeration event.
- COM_PowerUp/PowerDown are empty and READY can log despite partial init
  failure. Normal hotplug, suspend/resume and failed init are not yet proven
  production-quality behavior. Startup-only idle reset isn't a power policy.
- ADB diagnostics auto-run on every attach; synchronous file logging and
  polling remain on transport/cleanup paths. These are confounders to remove
  or disable in a controlled minimal-driver comparison, not proven causes.

## Next evidence needed before claiming a root cause or changing recovery

Instrument enter/exit/result/duration at callback return, DeactivateDevice,
each ClosePipe, parent/port retirement and HCD-level port status, using
minimal nonblocking diagnostics. Compare automatic reboot/hotplug with a
minimal no-ADB transport and, if feasible, no client binding. Independently
verify host power/port health. Do not infer that an unrelated EHCI bug applies
to this S3C6410 OHCI host. Do not blindly reset/unload HCD or free live buffers.

Fix lifecycle/cancellation/resource retirement first if implementation is
authorized. A safe asynchronous teardown must respect CE USB handle and DLL
lifetime; merely moving all close operations beyond callback return is not
automatically safe. WM6 reset remains a diagnostic recovery workaround,
not a required normal procedure or evidence that the client driver is sound.

Primary API reference for blocking cancellation:
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms902160(v=msdn.10)
Notification registration/unregistration contract (does not establish host
callback lock behavior):
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms919796(v=msdn.10)
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms923105(v=msdn.10)
