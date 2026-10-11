# WM6 device inspection — 2026-10-11

Read-only inspection through Joe's existing WMDC connection, with 32-bit RAPI. Connection/file-transfer implementation reused from Positron/scripts/device_gate.ps1; its deployment and cleanup workflow was not executed.

- Remote OS reports 5.2 build 20782. S3C6410 host and function driver files are present.
- PPP, TAPI, Unimodem, AsyncMac, NDIS, NDISUIO, TCPSTK, Winsock are present. NDIS/TAPI are in Drivers/Active. TCP/IP registry binds PPP.
- s3c6410_ohci2.dll is registered at Drivers/BuiltIn/OHCI and active as HCD1:. PhysicalPageSize = 0x40000 (256 KiB).
- USB8023.dll/RNDISMP.dll do not appear in the Windows directory inventory; no USB8023 client registration was found. Generic host RNDIS support is not established.
- RNDISFN.dll is registered under USB/FunctionDrivers/RNDIS and active. WMDC's DTPT configuration uses RNDISFN1. This does not establish host RNDIS support for ME3630.
- ZTEUSBCOM.dll/ZTEUSBManager.dll do not appear in Windows; no 6610_5170 USB LoadClients match or Software/ZTE key was found. No ZTE entry was found in the inspected Program Files or Windows/AppMgr directory listings. Current evidence does not show a functioning ZTE installation.
- LC6K_Modem.dll is present; matching USB VID/PID registration is decimal 6839_24576 (0x1ab7:0x6000). Six LC6K client entries are active; COM0, COM6, COM9 are explicitly named. Do not repoint this driver to ME3630 without compatibility evidence.
- ceconfig.h and USBLog.txt could not be copied because they were absent. ceconfig.h is not guaranteed to be shipped in a retail WM image. SYSGEN_STATE_NOTIFICATIONS cannot be inferred by searching for that build-variable name on the device. System/State exists but API/export validation remains outstanding.
- Existing USBDeviceInfo.txt was copied; it contains repeated bulk endpoint logs without enough identity information to attribute those entries to ME3630.

Next technical focus: validate a CE5-compatible ZTE driver binary and its imports; establish AT/MODEM ports before testing PPP. Adding a USB host RNDIS component remains a separate route. No device registry, driver installation, modem command, or dial-up configuration was changed by this inspection.
