param([switch]$Version2)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
if($Version2) {
 & $cl @flags /DPROBE_V2 "/Fo$build\me3630_probe_v2.obj" "/Fe$build\ME3630ProbeV2.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_probe.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'V2 DLL build failed'}
 & $cl @flags /DPROBE_V2 "/Fo$build\install_usb_probe_v2.obj" "/Fe$build\install_usb_probe_v2.exe" "$PSScriptRoot\install_usb_probe.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'V2 installer build failed'}
 return
}
& $cl @flags "/Fo$build\me3630_probe.obj" "/Fe$build\ME3630Probe.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_probe.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'USB probe DLL build failed'}
& $cl @flags "/Fo$build\install_usb_probe.obj" "/Fe$build\install_usb_probe.exe" "$PSScriptRoot\install_usb_probe.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'USB probe installer build failed'}
& $cl @flags /DREMOVE_PROBE "/Fo$build\uninstall_usb_probe.obj" "/Fe$build\uninstall_usb_probe.exe" "$PSScriptRoot\install_usb_probe.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'USB probe uninstaller build failed'}
