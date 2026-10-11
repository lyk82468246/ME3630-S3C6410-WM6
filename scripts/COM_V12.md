# V12 transport diagnostics; concurrent receive still fails

Independent version12 paths and instance query; fallback V11. Adds atomic
producer phase/cycle/timeout counters. Phase1=queue-space check, 2=USB transfer,
3=transfer returned/enqueue, 4=local backpressure. Snapshots are approximate,
not an atomic aggregate. TX logs only size and whether bytes equal AT+CR;
ATI size4 correctly yields expected_AT=0. No arbitrary payload is logged.

DLL SHA256: `bf30a67563f49fb2aafef067427561e7205de37d352564b597913d8529b95715`.
Hardware test completed 4/4 AT/ATI, 102 checks, 12 failures, including recurring
concurrent failures and additional backlog/removal failures. See focused logs
in evidence/2026-10-11-com-v12; this is not a passing driver.

Representative COM5 duplex: producer cycles6/timeouts4 at read beginning,
cycles26/timeouts24 after two zero-byte reads. Outgoing AT is verified at
tick515394; producer repeatedly completes timed-out reads, so this interval
does not support producer starvation or a dropped ring-buffer response.
USB transfer API reports successful writes, not proof of module receipt.

V13 is a single-variable experiment moving TX staging from the calling thread
stack to the driver-owned PORT allocation, still serialized by txIO with a
512-byte limit. No API/protocol/priority changes, and no assertion of a memory
mapping or cache-coherency root cause. Hardware results must decide its value.
