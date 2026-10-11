param([switch]$TestsOnly,[switch]$Version5,[switch]$Version6,[switch]$Version7,[switch]$Version8,[switch]$Version9,[switch]$Version10,[switch]$Version11,[switch]$Version12,[switch]$Version13,[switch]$Version14,[switch]$Version15,[switch]$Version16,[switch]$Version17)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
$version=4
if($Version5){$version=5;$flags+='/DCOM_V5'}
if($Version6){if($Version5){throw 'Select a single version'};$version=6;$flags+=@('/DCOM_V5','/DCOM_V6')}
if($Version7){if($Version5 -or $Version6){throw 'Select a single version'};$version=7;$flags+=@('/DCOM_V5','/DCOM_V6','/DCOM_V7')}
if($Version8){if($Version5 -or $Version6 -or $Version7){throw 'Select a single version'};$version=8;$flags+=@('/DCOM_V5','/DCOM_V6','/DCOM_V7','/DCOM_V8')}
if($Version9){if($Version5 -or $Version6 -or $Version7 -or $Version8){throw 'Select a single version'};$version=9;$flags+=@('/DCOM_V5','/DCOM_V6','/DCOM_V7','/DCOM_V8','/DCOM_V9')}
if($Version10){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9){throw 'Select a single version'};$version=10;$flags+=@('/DCOM_V5','/DCOM_V6','/DCOM_V7','/DCOM_V8','/DCOM_V9','/DCOM_V10')}
if($Version11){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10){throw 'Select a single version'};$version=11;$flags+=@('/DCOM_V5','/DCOM_V6','/DCOM_V7','/DCOM_V8','/DCOM_V9','/DCOM_V10','/DCOM_V11')}
if($Version12){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11){throw 'Select a single version'};$version=12;for($n=5;$n -le 12;$n++){$flags+="/DCOM_V$n"}}
if($Version13){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11 -or $Version12){throw 'Select a single version'};$version=13;for($n=5;$n -le 13;$n++){$flags+="/DCOM_V$n"}}
if($Version14){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11 -or $Version12 -or $Version13){throw 'Select a single version'};$version=14;for($n=5;$n -le 14;$n++){$flags+="/DCOM_V$n"}}
if($Version15){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11 -or $Version12 -or $Version13 -or $Version14){throw 'Select a single version'};$version=15;for($n=5;$n -le 15;$n++){$flags+="/DCOM_V$n"}}
if($Version16){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11 -or $Version12 -or $Version13 -or $Version14 -or $Version15){throw 'Select a single version'};$version=16;for($n=5;$n -le 16;$n++){$flags+="/DCOM_V$n"}}
if($Version17){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9 -or $Version10 -or $Version11 -or $Version12 -or $Version13 -or $Version14 -or $Version15 -or $Version16){throw 'Select a single version'};$version=17;for($n=5;$n -le 17;$n++){$flags+="/DCOM_V$n"}}
if(!$TestsOnly) {
& $cl @flags /DSTREAM_V3 /DCOM_V4 "/Fo$build\me3630_com_v$version.obj" "/Fe$build\ME3630ComV$version.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_com.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'COM DLL build failed'}
& $cl @flags "/Fo$build\install_com_v$version.obj" "/Fe$build\install_com_v$version.exe" "$PSScriptRoot\install_com.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'COM installer build failed'}
}
& $cl @flags /DCOM_V4 "/Fo$build\com_test_v$version.obj" "/Fe$build\com_test_v$version.exe" "$PSScriptRoot\stream_test.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'COM test build failed'}
