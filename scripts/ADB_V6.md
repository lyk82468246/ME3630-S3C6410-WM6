# V6: read-only module reboot diagnostics

V6 retains the V5 COM1/COM5 serial subset and 5-second startup delay. It adds a
bounded legacy ADB client for the exact 19d2:1432 MI4 ff/42/01 interface.
The only service it opens is:

```
shell:cat /proc/uptime /proc/sys/kernel/random/boot_id
```

No module configuration, SIM, network registration, RF mode or persistent power
policy is changed. AUTH stops the probe; no keys or approval bypass are used.
The CNXN banner is not published. Packet size, checksum, stream IDs and magic
are checked; transport waits are bounded. This is not a general-purpose ADB
client. Four samples per attachment, spaced four seconds apart, distinguish
USB reconnection from a new module Linux boot. Some inherited log strings still
say V4/V5; V6 READY and ME3630ComV6.dll identify the installed build.

Build with `scripts/build_com.ps1 -Version6`; deploy/test with the corresponding
`scripts/deploy_com.ps1 -Version6` and `-Version6 -Test` options in Joe's actual
32-bit PowerShell WMDC session. V5 remains the installer fallback.
Run `scripts/summarize_bootinfo.ps1` to interpret the captured driver log.
Separate WRTE frames may hold uptime and boot ID; associate them by attachment
and sample. Source files and diagnostic binaries are experimental, not a
production modem driver or PPP implementation.

## Verified on 2026-10-11

- Installation: dll_load=1, installed=1, error=0.
- Installed DLL SHA256: b03ebf3c5225d453c852efec8f2c1999cefac6622fba4c2c4966176cbfb7e58e.
- COM retest: commands 4/4, controls 32/32, failures 0.
- Three successive attachments yielded different kernel boot IDs and initial
  uptime 11.24, 11.68, 11.14 seconds. Module Linux reboots are confirmed;
  electrical reset versus watchdog, firmware or USB-triggered crash is not.
- The earlier V5 90-second test held a COM handle until failure, then reopened:
  4 generations, 27 successful queries, 4 losses. WM6 remained systemon in
  sampled power states. This is limited removal-path evidence, not an exhaustive
  race/lifetime test. The baseline monitor mislabeled CFUN queries as kind=2;
  its derived CFUN lines are correct. Source now fixes the label to kind=1.
  The rebuilt monitor has not been rerun; original evidence is unmodified.

## Power investigation

User confirmed no unplugging/SIM/power changes during the test, and that the
carrier is powered entirely by WM6 USB Host. This makes power a priority
hypothesis, not a demonstrated cause: no voltage/current waveform was captured,
and initial reboots already occurred before adding this ADB probe.

The vendor mini-PCIE hardware guide V1.5, sections 5.1-5.4, specifies 3.0-4.0V
at the module, transient peaks up to 2.3A under adverse RF conditions, and a
design peak capability of at least 2.5A. These are module-side design values,
not measured consumption during this AT test or a 5V USB input specification.
The WM6 port's actual current limit and the carrier's regulator are unknown.

Next controlled test: keep driver, USB data connection and SIM unchanged, change
only the power path using the carrier's documented input or a verified powered
hub without upstream VBUS backfeed, and repeat boot-ID monitoring. Inspect the
carrier model/schematic first. Never apply 5V directly to the module V_MAIN,
parallel two supplies, or improvise a USB Y-power cable. If stable power does
not resolve reboots, collect bounded read-only boot/reset/crash information and
compare with ADB disabled before implementing PPP.

## Primary references

- [AOSP legacy ADB protocol](https://android.googlesource.com/platform/system/core/+/android-4.4_r1/adb/protocol.txt)
- [AOSP checksum validation](https://android.googlesource.com/platform/system/core/+/android-4.4_r1/adb/transport.c)
- [Linux boot_id](https://www.kernel.org/doc/html/v6.12/admin-guide/sysctl/kernel.html)
- [Linux uptime](https://kernel.org/doc/html/v5.16/filesystems/proc.html)
- [GOSUNCN ME3630 mini-PCIE hardware guide V1.5](https://welinkjapan.co.jp/pdf/GOSUNCN%20Hardware%20Development%20Guide%20of%20ME3630%20mini-PCIE%20module_V1.5.pdf)
