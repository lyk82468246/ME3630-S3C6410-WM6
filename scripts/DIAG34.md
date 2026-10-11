# D34: zero-access companion opens, verified partial RAS progress

D33 actual trace shows a successful data open then a zero-access open while
`opened=1`. V18 rejected that second open. This is a measured blocking
condition, not an assertion that it is the sole cause of all633 failures.

D34 uses independent per-open contexts. Nonzero access remains exclusive;
zero-access metadata opens retain the port but never claim its data ownership.
Metadata close neither resets masks/queues nor clears another open's ownership.
Metadata read/write and mutating controls are denied; a small list of existing
readonly queries is allowed. Per-open read/write access is enforced.
No CTS, baud/flow, events, purge or timeout support is fabricated/expanded.

Production helper `me3630_open.inc` was tested in an isolated native WM6 test:
15/15 at316439,364660 and452564. Tests cover multiple metadata opens, exclusive
data rejection, close order, reopening data with an older metadata context,
query/mutation permissions, and balanced references. They do not prove every
concurrent I/O/detach race or full serial conformance.

Actual first D34 private identity34 at392098 accepted the companion open and
reached Get/SetCommState, SetCommTimeouts, SetCommMask and PurgeComm. RAS still
returned633. First installed DLL, pulled back from WM6, SHA256:
38A815F247EEA5947E30E49FA8319FB8C36003A893C2C5D106FC9D5C97AAAD5F.
It is preserved as evidence/2026-10-11-diag34/deployed-first.dll.

A rebuild retaining Open's failure code across diagnostic logging could not
overwrite that loaded DLL (device32). No forced unload was used. The local
base D34 build differs from that first installed file; do not identify it as
the binary used in the first trace.

D34R2 has a separate filename and identity3402. It adds numeric logging of
DCB, timeouts, mask and purge input, not serial content or credentials. Install
454879 succeeded, then natural re-enumeration allowed real attempt499066:

- initial19200 DCB flags2015 rejected; fallback115200 flags1011 accepted;
- read interval50, multiplier1, constant50, write multiplier5, constant500;
- event mask1f9 rejected;
- purge0c (TXCLEAR|RXCLEAR) rejected;
- data and metadata closes both observed, RAS633, no PPP projection.

R2 SHA256094A0DD81DE09054AA6876CE99BDBDFB12717799C9F24DFDF0B18DB1189B9AE6.
Current exact USB match points to R2. Original V18/D33/D34 stay available.
No extra user-unapproved reset was used during D34/R2 testing. User saved
entry and all LeadCore/WMDC settings are retained.

Next work: implement measured timeout and FIFO-clear semantics, and determine
truthful handling of the requested CTS/BREAK/ERR event subscriptions. Initial
hardware handshake DCB cannot simply be acknowledged because CTS is unknown.
Actual DCB fallback is already accepted; changing settings is not yet justified.
