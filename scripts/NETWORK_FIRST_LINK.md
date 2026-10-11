# First-link work: valid China Telecom SIM, reconnect rather than stability gate

User explicitly requested ignoring long-term stability for now, retrying
after module removal/reboot, and confirmed a valid China Telecom SIM.
The driver already tears down disconnected instances and activates fresh
COM1/COM5 on a new USB attachment. It cannot recreate an absent USB device.
New application-level probes retry fresh handles; the RAS attempt retries
only our named modem, never the pre-existing Cellular Line.

## Actual measurements

network_probe tick3688802: instance18, ten read-only commands complete in
2456ms. CFUN1, CPIN READY, CSQ99/99, COPS0, CREG0/2, CEREG0/2, CGATT0.
CID1 IPV4V6/ctnet and CID2 IP/ctnet already configured, both inactive.
No APN/RF/SIM/IMEI setting was changed. This proves SIM detection and current
APN, not usable registration or data connectivity. Later acquisition windows
have zero ME3630 handles, including after the user's manual reconnect.

Initial RAS probe tried desktop rasapi32.dll and returned126. Corrected
probe resolves the actual CE RAS exports from coredll (fallback ppp.dll),
successfully enumerating only one modem, Cellular Line. This is NOT ME3630.
One early collection contained no COMPLETE marker and is preserved as
incomplete, never accepted as success. The runner now polls a fresh-start
completion marker up to75s rather than assuming a fixed sleep means success.

Driver capture last detach tick3720349, both RX/notification joins0; subsequent
captures have no new ATTACH. Read-only Active snapshot shows zero ME3630
ports, while OHCI and our V18 match remain. Thus the current blocker is
missing host enumeration, not a recoverable stale COM handle. WMDC remains
connected. No WM6/host reset was performed.

## Scoped Unimodem registration

register_unimodem.exe changes only Drivers/ME3630ComV18/Port2: serial IClass
CC5195AC-BA49-48a0-BE17-DF6D1B0173DD, Unimodem subkey Tsp, DeviceType1,
FriendlyName ME3630 Test COM5 and a25-byte DevConfig. Layout comes from the
user's vendor USBManager.reg; only baud bytes are changed from19200 to115200.
Other configuration flags are retained, not claimed to have verified flow
semantics. First registration tick3829231 and updated IClass registration
tick3975231 both return0. Registry success does not prove TAPI enumeration;
it is intended to take effect on next activation. Vendor binaries, existing
LeadCore/Cellular Line and function-side WMDC/RNDIS were not modified.

## Bounded RAS attempt

ras_attempt.exe selects only exact modem name/type, retries enumeration
three15s windows, and limits overall loop to60s. If found, it creates a
unique temporary ME3630-Test-tick entry, PPP/IPv4, dial *99***2# (existing
IP/ctnet CID2),115200 and vendor configuration. It asynchronously observes
RAS state up to15s per attempt; only successful IPv4 projection counts as
PPP success, not internet connectivity. It hangs up ONLY its own attempt
and deletes ONLY its uniquely created entry. No credential is copied from
existing connections. Default-gateway changes would be transient during
an actual connection. Currently the modem has not yet been enumerated, so
no successful PPP/IP or internet request is claimed.

Actual RAS program tick4125692: three enumeration windows all report our
named modem absent; COMPLETE connected0 elapsed45352ms. It never reaches
entry creation or RasDial, so no cellular data session or route change is
claimed. Existing modem/device names are not selected as a fallback.

Native builds and new PowerShell syntax checks pass. System RAS calls can
themselves block beyond the loop deadline; the deadline is not a hard bound
on individual OS calls. No force-termination/reset of unrelated device
processes is added. Frontend runner validates actual completion/result.

Primary API references:
- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa450833(v=msdn.10)
- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa450836(v=msdn.10)
- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/aa450834(v=msdn.10)
- https://developer.toradex.com/windows-ce/knowledge-base/wince-serial-ports/

Next prerequisites: restore host enumeration (a WM6 soft reboot needs user
approval because it ends apps/WMDC), verify the named TAPI/RAS modem, inspect
actual serial IOCTL requirements, then reattempt registration/data dialing.
Software retry does not eliminate the need for the USB hardware to reappear.
