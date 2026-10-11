# V9 independent receive queue

V9 inherits V8's RXCHAR mask/cancellation subset and V7 TX concurrency. The
installer retains V8 as fallback. USB matching, COM1/COM5, startup delay, bounded
read-only ADB boot probe and LeadCore/WMDC exclusions are unchanged. No dial,
RF/APN/SIM configuration or persistent power policy changes.

Each port now has one background USB receive producer and a 4096-byte FIFO.
The producer performs bounded 200ms bulk reads into local 512-byte staging,
then pushes into the ring under the configuration/queue lock. It posts separate
reader and event notifications. ReadFile serializes competing application
readers with RX lock, but WaitCommEvent no longer takes that lock or touches
USB. Both operations can wait simultaneously and observe the same arrival;
event bits coalesce, while the reader consumes bytes only once.

The shared lock protects ring head/count, mask generation, pending bits and
receive-failure state. Notification predicates are checked and reset under this
lock, preventing a producer or mask change from racing between check/reset.
Reader timeout is a per-call snapshot. First-byte mode returns available data;
total-only mode accumulates to request size or deadline. A receive failure is
reported rather than producing fictitious data; already copied bytes may be
returned as a partial successful read, with the failure visible on a later call.
Device removal is checked before copying old bytes.

Backpressure: no new USB receive is issued unless at least 512 bytes are free.
The local ring never overwrites unread bytes. This may leave up to 511 bytes
unused; it is NOT a guarantee against overflow in the module's own USB buffers
or loss under sustained traffic, and no hardware/software flow control exists.
Receiving continues while the application is closed; queued URCs may survive
close/reopen. Purge and per-open queue policies are not implemented yet.

Removal signals parent stop, joins the attach worker, then joins each RX worker
before deactivating/closing port pipes. RX producer holds a port reference until
exit. Join has a 5-second bound; a stuck worker retains its base references and
pipes rather than being terminated or freed underneath. Event waiters retain
their own references. This is a limited prototype safety policy, not exhaustive
unload, Close/read, suspend/resume or stuck-host verification.

Private version query 0x00223ffc returns DWORD9. The native test verifies each
opened instance. Build/deploy with build_com.ps1 -Version9 and
deploy_com.ps1 -Version9 [-Test] in Joe's actual 32-bit WMDC session.

Expected suite: 102 checks, 4/4 AT/ATI, zero failures, COMPLETE:

- 6 native ring tests use production me3630_ring.inc verbatim: empty pop,
  exact-capacity fill, rejected overflow, partial pop, wrapped push, wrapped FIFO.
- 68 inherited serial/duplex/event checks.
- 14 additional checks per port: competing reader and event waiter both get
  the AT arrival, and a second wait with unread response does not block draining
  the remaining bytes; mask cancellation releases that waiter.

These do not stress physical 4KB queue saturation or high-rate traffic. No
modem-line interrupt decoding, GetCommModemStatus, error/status API, purge,
SetupComm, EV_TXEMPTY or PPP has been added. Module kernel reboots remain a
separate unresolved issue.

Installed DLL SHA256:
`2e678265c1cf3ec43df3575edf979188d7bf59a90f6ce1c330869a8d43f5515c`.

## Actual device result: FAILED, not a validated replacement for V8

Run start_tick=14794842 explicitly opened version9 on COM1 and COM5.
AT/ATI passed 4/4, but 102 checks completed with SIX failures. Both ports
failed the inherited background-reader duplex check and the new simultaneous
reader/event pair. Writes completed in 2ms; background reads returned zero
bytes after about 4s. Event waiters were eventually cancelled by the test.
The six ring checks and unread-backlog regression passed. Preserve this result
in evidence/2026-10-11-com-v9/test.txt; do not change assertions to accept it.

Later detach logs show both RX workers joined successfully (wait=0), but this
does not establish the cause of the earlier receive failures. V10 adds focused
queue diagnostics; it is not a claimed fix. V8 remains the last fully passing
hardware suite, with its documented concurrency limitations.
