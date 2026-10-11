# V18: partial modem-line events

V18 extends V17 with EV_DSR/EV_RLSD/EV_RING while retaining RXCHAR and the
explicitly partial private state query. CTS, full GetCommModemStatus, flow
control, UART errors, TXEMPTY, overlapped I/O and PPP remain unsupported.
No dial/RF/APN/SIM/IMEI command, host reset or LeadCore/WMDC change is added.

The actual validated interrupt packet updates state and events under the
same port lock. DSR and DCD events require a change from a previously known
sample; ring is reported only on a known low-to-high transition. The first
accepted sample establishes a baseline without synthesizing an edge. Invalid
or repeated samples generate no new edge. Disabled events still update the
state baseline; enabling a mask does not manufacture old transitions.
This deliberately restricted ring behavior is not a claim of universal
CDC/Unimodem compatibility. API meanings were checked against Microsoft:
https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-waitcommevent

Edges coalesce into pending bits AND the current mask, preserving previously
pending RXCHAR. The interrupt producer wakes the existing WaitCommEvent
event. Mask changes increment the existing cancellation generation and clear
pending bits; CTS/other unsupported masks reject before mutation. RXCLEAR
removes only RXCHAR, leaving pending line events untouched. A notification
worker failure invalidates known-state and wakes waiters; waits requesting
line events fail with that error, while RXCHAR-only waits remain independent.
Removal still fails pending waits as disconnected. Generation cancellation
is checked before notification failure; RX pump errors retain priority.

No synthetic notification is injected into the installed USB driver. The
separate line_unit executable has fake samples but directly includes the
production state-update and event-wait helpers, no USB or serial operations.
Its 21 checks cover baseline/edges/coalescing, filtering, malformed input,
RX event preservation, duplicate waiters, cancellation, asynchronous wakeup,
line-failure isolation, disconnect and reference balance. Actual run
tick2946472: CHECKS21 FAILURES0 COMPLETE. The initial18-check version also
passed before expanding coverage; published binary is the21-check version.

Hardware suite adds six checks per port (162 total): accepted line mask,
roundtrip, atomic CTS rejection, waiter creation, prompt cancellation and
wait completion with zero or a genuine supported edge. It does not require
or fabricate positive line transitions. All opened handles verify version18.
Windows SDK compatibility syntax was checked for V6/8/10/14/15/16/17/18,
plus all PowerShell scripts. Actual hardware results follow after completion.
Persistent module reboots, physical line transitions and PPP remain separate
unresolved work; software-only edge tests are not hardware carrier evidence.

## Hardware results

DLL SHA256: 6bfaeecc97eacde7cffea72ae194e724e45b556defd39a392facb3a2e2b04034.
Installer dll_load1 installed1 error0. First native test tick2982819 verifies
both handles as18, AT/ATI4/4 CHECKS162 FAILURES3 COMPLETE, NOT a full pass.
COM1's new mask/cancellation checks pass (wait227ms/cancel17ms, mask0).
COM5 disconnects in its final line-event section: RX_EXIT2993666 error1167,
DETACH2993725, both notification and RX joins wait0. CTS-mask atomicity and
subsequent cancellation checks fail on the removed handle. First outputs
are retained as driver-first/test-first; the device was not reset or manually
reopened within the failed tests. A second bounded fresh test follows.
