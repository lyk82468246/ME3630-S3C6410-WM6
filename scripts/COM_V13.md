# V13 driver-owned TX staging experiment

V13 inherits V12 diagnostics and changes only TX staging lifetime/location:
PORT.txStage[512], allocated with the port and protected by txIO, replaces
the per-call stack array. The buffer is retained through transfer completion,
abort/close and port teardown. RX producer/ring/event protocol is unchanged.
The older paths retain their original stack staging and 512-byte limit.

Separate DLL/installer/test/registry/log names; private instance returns13;
fallback V12. Installation success is NOT hardware-test success. No arbitrary
device commands, driver priority, SIM, RF, APN, power or WMDC changes.

This tests a buffer-location hypothesis, not an established cause. It does not
provide an explicit slot mapping, DMA allocation or cache flush and must not
be described as solving those mechanisms without further evidence.

Actual DLL SHA256: `f11e0414a74f3e6a7c9eb958d4c88013a84c9957921f0e90a98fd42ba2689894`.
Both ports verified version13; hardware test AT/ATI4/4, CHECKS102 FAILURES6
COMPLETE. The same concurrent failures recur; both backlog regressions pass.
Moving TX staging did not fix the observed failure. Focused evidence is in
evidence/2026-10-11-com-v13. V14 next tests reduced RX abort frequency.
