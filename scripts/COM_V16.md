# V16: read-only interrupt notification capture

V16 inherits V15's COM subset unchanged. It captures interrupt IN endpoints
on MI1 and MI2 only (19d2:1432, alternate setting zero, packet <=64 bytes).
No MI0/MI3 pipe, dial command, APN/SIM/IMEI setting or host reset is added.
GetCommModemStatus remains unsupported; events remain RXCHAR only.

Each port has a separate interrupt pipe, worker and local 64-byte USB buffer.
Pending interrupt requests use a 30-second idle deadline, not the bulk pump's
5-second deadline. Removal signals the existing stop event, joins notification
and RX workers before closing pipes, and retains a stuck port rather than
freeing its USB buffer/context. The notification worker holds its own port
reference. Open/thread failure is logged without disabling bulk COM.

This is capture, not a CDC parser: packets are hex logged, not interpreted or
used to assert CTS/DSR/DCD/RI. Unknown input, fragmentation, line transitions,
notification flooding and error recovery still need validation. The existing
shared file logger is not serialized; occasional interleaved lines are visible
in the trace. Do not infer notification absence from a corrupted/missing line.

Primary cross-check: Linux option driver's notification callback recognizes
A1/20 and reads DCD/DSR/RI from its payload, but assigns CTS active without a
corresponding received CTS bit. We do not copy that assumption or its code:
https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/serial/option.c

## Actual device evidence

DLL SHA256: f31e33db81238b259d33990dda30ede14ee16d5122cee2f6e35e65b63e10124d.
Installer: dll_load=1 installed=1 error=0. The initial read-only snapshot
had zero active ME3630 ports; automatic re-enumeration subsequently activated
V16, without asking the user to reconnect or resetting WM6.

First test tick2076812: both opened handles verify INSTANCE_VERSION=16;
AT/ATI 4/4, CONTROL_CHECKS=138 FAILURES=0 COMPLETE. Previous V15 raw ATI
and full registry inventories remain local; published test logs redact IMEI.

MI1 (endpoint83) and MI2 (endpoint85) each produced eleven 10-byte packets
in the first enumeration window:

    MI1: a1 20 00 00 01 00 02 00 00 00
    MI2: a1 20 00 00 02 00 02 00 00 00

These match the expected notification header/interface/two-byte payload
layout, but all observed payloads are zero. No positive carrier/ring/DSR
transition or CTS support is established. DTR/RTS changes in the existing
suite are followed by repeated notifications; correlation is not proof of
their semantic meaning. No modem-line API is claimed complete.

Detach tick2110619: both NOTIFY_EXIT lines, both NOTIFY_JOIN wait0 and both
RX_JOIN wait0 are recorded. A later enumeration returns startup uptime11.31
and a different boot_id; adding interrupt capture has not solved module
reboots. First focused trace/test are retained as driver-first/test-first.

Production queue fake-transport regression ran afresh at tick2101754: 13/13
pass, including dual-reader cancellation; it does not exercise interrupt USB.
Driver/test/installer syntax checked for V6/V8/V10/V14/V15/V16 and all
PowerShell scripts parsed successfully.

Second hardware run tick2145269 also verifies both handles as version16, AT/ATI4/4,
CONTROL_CHECKS138 FAILURES0 COMPLETE. These are two complete short test
windows, not continuous service or a solution to the observed module reboots.
The final cumulative trace has 46 notification lines, all with zero payloads.
driver-first is a cumulative snapshot pulled while launching the second run,
not a trace restricted to the first run; test-first is the first run's output.

Next: strict notification parser with malformed-input tests and an explicit
known-state mask (CTS unknown), then status/event semantics. Fix shared trace
serialization in a new independently versioned build. Real PPP and power/
firmware reboot diagnosis remain separate, unfinished work.
