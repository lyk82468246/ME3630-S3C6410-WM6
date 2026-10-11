# USB transport prototype

ME3630ProbeV2.dll is a CE5.2 ARM USB client, built with the installed VS2008 ARM compiler and WM6 SDK. It is not yet a serial stream driver. No COM port is exposed. The only transmitted AT command is `AT\r`; it does not dial or modify modem settings.

Device matching is exactly VID19d2/PID1432. RegisterClientSettings is called through the existing USBD.DLL. The driver takes control of that composite device, logs its active descriptors, and probes only MI1/MI2. Other interfaces, including the network interface, are not implemented.

Measured endpoints:

| Interface | Bulk IN | Bulk OUT | Interrupt IN |
|---|---|---|---|
| 1, AT | 0x82 | 0x02 | 0x83 |
| 2, MODEM | 0x84 | 0x03 | 0x85 |

V2 waits 20 seconds after enumeration, attempts SET_CONTROL_LINE_STATE with DTR and RTS asserted for the interface, sends AT, and performs one bounded bulk read. Both interfaces returned OK on the target device. This does not isolate whether the startup delay or control request caused improvement over V1.

Build: `scripts/build_usb_probe.ps1 -Version2`.
Deploy/registration: run `scripts/deploy_usb_probe.ps1 -Version2` in 32-bit Windows PowerShell through the actual Joe/WMDC session.
Read current log without installing: `scripts/deploy_usb_probe.ps1 -PullOnly`.
Explicit unregister: `scripts/uninstall_usb_probe.ps1` (also removes the V2 registration because both versions use the same exact client ID/settings). DLLs/logs are retained; an already attached device must detach for its loaded client to unload.

Device files: Windows/ME3630ProbeV2.dll; Temp/ME3630-install-v2.exe; Temp/ME3630-install-v2.txt; Temp/ME3630-usb-probe.txt. Source changes must be rebuilt and deployed under a new DLL filename when an older version may still be loaded.

Next engineering work: serial stream COM_* interface, collision-free port allocation, CE5 caller-buffer handling, DCB/timeouts/serial IOCTLs, safe synchronization between reads/writes and USB removal, receive buffering and Unimodem integration. No PPP-ready claim is warranted yet.
