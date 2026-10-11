# V14 receive abort-frequency experiment

Inherits V13, changing only background USB receive deadline from 200ms to
5000ms. Application ReadFile timeouts still use the independent ring-reader
deadline. The USB wait loop checks stop as before, so device removal need not
wait for the full idle deadline. Join safety policy is unchanged; this is not
proof of a guaranteed abort bound inside the Host driver.

Separate DLL, instance14, installer, registry and test/log paths; V13 retained.
This tests whether repeated cancellation contributes to lost input. No claim
of a fixed USB cancellation race without hardware evidence; no RF, APN, SIM,
modem reset, priority, power or unrelated driver changes.

DLL SHA256: `dc781f4093983e13e020b2859bd697348c40169e168fb116cfe3270b66fe21c6`.
First fresh hardware run tick790449 verifies instance14 on both ports:
AT/ATI4/4, CHECKS102 FAILURES0 COMPLETE. Background duplex reads receive
six bytes in 336/338ms; simultaneous reader/event checks receive six bytes
in 166/169ms (reader lifetime), events372/375ms. Backlog drains85-88ms.
The suite also verifies 350ms empty application reads despite the longer
USB producer deadline. First results/trace are preserved separately.

This supports frequent idle receive cancellation as an important contributor
to the observed concurrency failure, not proof of a particular Host/firmware
race. The TX buffer-location experiment alone failed in V13. Module reboot/
detach still occurs outside the successful test interval; V14 is not a stable
production modem driver. Modem-line status, purge, flow control and PPP remain
unimplemented. Second independent hardware run is recorded after completion.

Second run tick875248: 2/4 commands, 97 checks, 26 failures COMPLETE. COM1
opened version14 at875256 then RX failed875659 /detach875700 during control
testing, so later operations on that handle fail. COM5 opened a newly enumerated
version14 at889007 and its duplex/concurrent/backlog checks pass (duplex331ms,
concurrent reader168ms, six bytes each). This is NOT a second complete pass.
Second test/trace are preserved separately. A bounded third run follows.

Third fresh run tick945045 again verifies instance14 on both ports and passes
4/4 commands, 102 checks, zero failures COMPLETE. Duplex332/437ms, writers
18/114ms; simultaneous reader182/164ms, events389/372ms, all six-byte replies.
Two complete passing runs, with a reboot-interrupted failed run between them:
do NOT describe these as consecutive successes or long-term stability.
