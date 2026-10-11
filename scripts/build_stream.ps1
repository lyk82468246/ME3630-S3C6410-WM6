$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
& $cl @flags /DSTREAM_V3 "/Fo$build\me3630_stream_v3.obj" "/Fe$build\ME3630StreamV3.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_stream.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Stream DLL build failed'}
foreach($name in @('install_stream','stream_test')) {
 & $cl @flags "/Fo$build\$name.obj" "/Fe$build\$name.exe" "$PSScriptRoot\$name.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw "$name build failed"}
}
