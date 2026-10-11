# V18 D33: RAS633 diagnostics, not a compatibility fix

The first real HWND-based RAS attempt returned633. This does not establish
another application owns the port. Existing trace shows a COM5 open but no
subsequent serial control calls, and did not log close operations.

`ME3630ComV18D33.dll` is a separately named V18 logging variant, retaining
private version18 and the V18 registry paths/transport semantics. D33 markers,
not the version query alone, identify the diagnostic implementation.

- Open entry records port, process, access/share and current open state.
- Close records process, current open state and reference count.
- Control entry is logged before the existing checks/lock; normal control
  return records result/error. The preexisting early wait/dead-port paths do
  not emit the normal return marker.
- No unsupported request is made successful, no CTS is fabricated, no DCB,
  timeout, queue, RF/APN, modem-status or power policy is relaxed.
- No serial data or credentials are newly logged.

Installer validates the existing exact19d2:1432 client match currently names
V18 or D33, loads the new DLL to verify it, and changes only that match's DLL.
Port1/Port2 DLL values are refreshed by the next actual attach worker. Existing
Unimodem, IClass and user's saved entry are preserved. Original V18 DLL stays
available. A loaded instance is not replaced; installation is not activation.
To revert, restore that one match's DLL to ME3630ComV18.dll and reattach; its
worker restores the port DLL values. No host or global TAPI reset is performed.

SHA256 F4D35E67AA67AC8E5EEF3BA5047C8A04548AD7758D963364355213CF753220D8.
Build and install443534 succeeded; read-only Active snapshot still has zero
ME3630 port blocks. No D33 hardware test or633 fix is claimed.

The saved-entry inspect mode now queries only ME3630 TAPI lines, reading caps
and config metadata, opening a monitor line, then closing/shutting down. No
lineMakeCall, device config change or credential query. Actual329898 result:
own line8, name ME3630 Test COM5, DATAMODEM media10, features8, caps1e0;
config684 bytes, monitor open success, portname query80000048
(LINEERR_OPERATIONFAILED). A monitor open can succeed while USB is absent;
this does not prove port health or identify the exact633 cause.

Primary API reference:
https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms894331(v=msdn.10)
and installed WM6 SDK tapi.h. Full registry snapshots remain local.
