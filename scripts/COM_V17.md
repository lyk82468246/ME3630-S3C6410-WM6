# V17: validated partial notification state

V17 inherits the V16 transport and adds a shared strict parser, a private
snapshot query and serialized trace writes. No dial, APN/SIM/IMEI setting,
host reset, Unimodem registration or change to LeadCore/WMDC is introduced.
Standard GetCommModemStatus and non-RXCHAR events remain unsupported.

## Packet validation and state

Accept exactly 10 bytes: A1/20, wValue0, matching MI1/MI2, wLength2,
high payload byte0 and no reserved payload bit7. Wrong/null/short/oversize/
other-interface packets are rejected without modifying parser output values.
This intentionally targets the observed firmware, not arbitrary CDC framing.
Fragmented notifications are rejected; no reassembly is implemented.

Payload bits0/1/3 map to RLSD/DSR/RING. Other defined low bits are retained
as raw data, not promoted to CTS or injected as UART errors. CTS has no
received representation here and is explicitly unknown. These mappings
are cross-checked against Linux option.c, not copied implementation code:
https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/serial/option.c

Private IOCTL 00223ff8 takes zero input bytes and exactly 32 output bytes:
eight DWORDs in order: size, known, status, raw, accepted, rejected, tick,
error. size=32; known=E0 only after a valid notification; status contains
only the known modem bits. tick timestamps the last accepted notification.
Before the first notification known=0 (not all lines inactive). A bad packet
increments rejected and leaves the last accepted state/tick intact. The
snapshot is historical, not proof the module or carrier is currently healthy.
All snapshot mutation and query copying use the port configuration lock.
Notification worker exit invalidates known and records a disconnect/I/O
error; an already disconnected COM handle itself rejects IOCTL access.

Trace locking covers open/seek/stamp/message/close, serializing threads in
one loaded DLL process, initialized/released at process attach/detach. It
does not coordinate different processes. Formatting is performed before the
lock; no code acquires a port lock while holding the trace lock. This avoids
the previous multi-thread append collision without changing USB deadlines.

## Verification

notify_unit.c includes the actual production parser, has no USB/serial I/O,
and ran on WM6 tick2422104: 23 checks, zero failures. It covers line mappings,
MI2, nulls, wrong interfaces, truncated/oversized data and every validated
header/reserved byte, asserting rejected input cannot overwrite prior outputs.
Positive synthetic line bits test software only, not physical carrier/DSR/RI.

build_com.ps1 -Version17 succeeded; DLL SHA256:
e274c9ec1728ff57afbfbe3b4959e950c75210e45dea906b1a7dfd332691da9e.
Installation dll_load1 installed1 error0. Hardware suite adds six checks
per port to V16's138 (expected150): snapshot ABI/received state, CTS unknown,
status subset, short output rejection, nonzero input rejection, and continued
GetCommModemStatus rejection. Every tested handle verifies instance17.

V6/V8/V10/V14/V15/V16/V17 driver/test/installer C syntax and all PowerShell
scripts parse successfully. Long-term stability, physical line transitions, notification
error recovery, flow control, line events and PPP remain unverified/unfinished.

## Actual hardware run

First fresh test tick2473910: both opened handles verify INSTANCE_VERSION17;
AT/ATI4/4 CONTROL_CHECKS150 FAILURES0 COMPLETE. Each snapshot reports
knownE0, status00, raw00, accepted11, rejected0, error0. Short output and
nonzero input reject with87; GetCommModemStatus rejects with50 as intended.
Both notification threads and RX threads join with wait0 after module detach.
No physical positive line transition has been observed. The trace and first
test output are retained separately before starting the second run.
The first trace has24 parsed notification lines and no multiple-tick line
collisions. This is a bounded observation, not a proof that all logging races
are eliminated. After the suite, interrupt MI1 reports USB error257 at
tick2484207, followed by detach2484280 and successful joins. The error is
preserved; its cause is not identified and reboot stability is not improved.

Second test tick2557919: AT/ATI4/4, CHECKS150 FAILURES47 COMPLETE, NOT a
second pass. COM1 loses USB at RX_EXIT2559397 /DETACH2559571 during duplex
testing; later operations on that same handle fail1609. The suite does not
reopen a failed handle to hide this. COM5 subsequently opens a fresh V17
instance and its checks pass, including accepted11/knownE0. Failed outputs
are retained as test-second/driver-second before one final bounded retest.

Third fresh test tick2643684: both instances17, AT/ATI4/4 CHECKS150
FAILURES0 COMPLETE. Both snapshots again show accepted11/rejected0,
knownE0/status00/raw00/error0. The final cumulative trace contains66 parsed
notifications and no lines with multiple tick markers. First and third are
complete passing windows separated by the second failed window, NOT
consecutive passes or long-term stability. All three outputs are retained.
