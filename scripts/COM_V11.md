# V11 checked-wait diagnostics: six hardware failures remain

Independent DLL/installer/test paths, private instance version11, fallback V10.
Adds current process/thread priority diagnostics, first receive-event wait
result, explicit ResetEvent/WaitForSingleObject failure propagation. No priority,
USB protocol, SIM, RF or network changes. DLL SHA256:
`b34417e85a55eaf1044d2df362a2dedc2d32990940c7da63d92f638a24ab2b78`.

Actual hardware result: AT/ATI4/4, CHECKS102 FAILURES6 COMPLETE. Both ports
verify instance11. The concurrent reader/event failures recur; backlog passes.
Producer and caller contexts report the same process ID and priority251.
Recorded initial waits return WAIT_TIMEOUT (0x102), or success when data arrives;
no WAIT_FAILED or ResetEvent failure was reported. This does not support the
invalid-event-handle hypothesis; only the first wait of each read is logged.
No priority change is justified by these measurements alone.

V12 adds atomic producer phase/cycle/timeout counters and checks whether the
three outgoing bytes equal AT+CR, without recording arbitrary payloads. This
distinguishes a stopped producer from one continually timing out, and validates
the contents supplied to the USB write rather than just its reported length.

Evidence: evidence/2026-10-11-com-v11/driver.txt and redacted test.txt.
API reference for failed waits returning WAIT_FAILED, not a timed wait:
https://learn.microsoft.com/en-us/previous-versions/ms915517(v=msdn.10)
