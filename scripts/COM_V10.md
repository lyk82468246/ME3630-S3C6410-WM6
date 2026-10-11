# V10 diagnostic build: installed, USB activation not yet verified

V10 inherits the V9 receive queue unchanged. It adds Q_PUSH, Q_READ_BEGIN,
Q_POP and Q_READ_END records with port, thread ID, transient context pointer,
byte counts and timing. It does not log payloads or device identifiers.
Private instance IOCTL returns DWORD10. DLL, installer, registry paths and
test logs are versioned independently; older binaries are preserved.

DLL SHA256: `8df2116a79abec61217e447119759d951c41ee5843c629c36bdfa0b667b7f67d`.
Installation returned dll_load=1 installed=1 error=0. LoadClients exact match
6610_5170 (19d2:1432) currently points to ME3630ComV10.dll.

After the user unplugged/replugged and waited 30s, deployment still found no
V10 READY within its bounded 40s check and did NOT launch the USB test.
A read-only Active snapshot found no ME3630 ports; HCD1 remains active.
The V10 driver log has only INSTALL tick15063535, no ATTACH. This does not
prove power loss, Host failure, a changed PID or a driver incompatibility.
USBDeviceInfo.txt is a historical endpoint log, not a live topology snapshot.
The user reports no driver-name dialog; the board has only a traffic LED,
which illuminates during high-rate data, so darkness is not power evidence.

## Independent queue isolation result

queue_unit.c runs production ring/pump/read helpers with a deterministic fake
transport in a single WM6 process. No USB, serial port, AT command, driver
registration or persistent configuration is touched. run_queueunit.ps1 builds
and launches it through Joe's actual 32-bit WMDC/RAPI session, verifies a fresh
start_tick and a complete result.

Actual run tick15505883: CHECKS=10 FAILURES=0 COMPLETE. Verified background
reader receives six simulated bytes, RX notification survives consumption,
one-byte reads retain the remaining five bytes, total deadline returns partial
data, empty deadline returns zero, stop releases the reader and joins the pump.
Evidence: evidence/2026-10-11-queue-unit/test.txt.

This does NOT exercise a real USB transport, stream-manager call contexts,
cross-process mappings, USB cancellation races, saturation or hardware reboot.
It narrows the investigation but does not clear V9's six hardware failures.

Next: restore an actual enumerated module, verify instance10 on both ports,
then correlate producer/read diagnostics during the 102-check suite. A WM6
soft reset may be needed to distinguish a stuck Host state, but is not done
without user approval because it interrupts WMDC and other device programs.
No APN, RF, SIM, IMEI, LeadCore, WMDC or power policy changes were made.
