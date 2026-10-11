# V3 diagnostic stream

This is a CE5.2 diagnostic USB + stream DLL, not yet a serial/PPP driver.

- Exact USB match: 19d2:1432. MI1 -> M361:, MI2 -> M362:.
- Uses the verified 20-second readiness delay and DTR/RTS initialization.
- `ActivateDevice` puts a per-port pointer in ClientInfo; M36_Init acquires the
  Device Manager reference. One opener per port; Read/Write use a local 512-byte
  staging buffer and a 3-second transfer deadline. Larger writes return partial
  counts and must be retried by callers.
- Removal sets the stop event, joins startup, deactivates children, and closes
  pipes under the I/O lock. Port/open references retain the parent context until
  release. A startup-thread shutdown failure retains context rather than freeing
  a potentially live object. Unplug-with-open-handle behavior is not yet verified.
- Unsupported IOCTLs and seek return ERROR_NOT_SUPPORTED. No DCB, configurable
  serial timeouts, status/event handling, full-duplex buffering or suspend/resume
  support. Do not point Unimodem/PPP at these diagnostic endpoints.

Build: scripts/build_stream.ps1. Install: scripts/deploy_stream.ps1 using actual
Joe context and 32-bit Windows PowerShell. Installation preserves V2 files and
falls back to V2 registration if V3 installation fails. A physical module replug
is required to replace an already-loaded USB client. Do not overwrite a loaded
DLL; future transport changes must use a new filename/version.

Test: deploy_stream.ps1 -Test. Sends only AT and ATI via CreateFile/WriteFile/
ReadFile. Waits 25 seconds with SystemIdleTimerReset (diagnostic only, no permanent
power-policy changes). COMMANDS_PASSED must be 4/4, with COMPLETE, and response
contents must still be reviewed. Raw ATI logs are ignored; published logs redact
IMEI. Executable copies use unique names to avoid replacing a locked image.

References used for the CE5 stream ABI and activation context:

- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms923700(v=msdn.10)
- https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms896106(v=msdn.10)

Do not use desktop NT serial IOCTL numbers as substitutes for the CE Pegdser.h
definitions. The installed WM6 SDK has no Pegdser.h; resolve the CE ABI before
implementing standard COM entry points.
