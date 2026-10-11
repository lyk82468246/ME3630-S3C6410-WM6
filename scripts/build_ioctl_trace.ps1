$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
& $cl @flags /DTRACE_DLL "/Fo$build\ioctl_trace_dll.obj" "/Fe$build\ME3630IoctlTrace.dll" "$PSScriptRoot\ioctl_trace.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\ioctl_trace.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Trace DLL build failed'}
& $cl @flags "/Fo$build\ioctl_trace_exe.obj" "/Fe$build\ioctl_trace.exe" "$PSScriptRoot\ioctl_trace.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Trace EXE build failed'}
