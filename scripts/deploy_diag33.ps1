param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
$driverFlags=$flags+@('/DSTREAM_V3','/DCOM_V4','/DCOM_DIAG33');for($n=5;$n -le 18;$n++){$driverFlags+="/DCOM_V$n"}
& $cl @driverFlags "/Fo$build\me3630_diag33.obj" "/Fe$build\ME3630ComV18D33.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_com.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'D33 driver build failed'}
& $cl @flags "/Fo$build\install_diag33.obj" "/Fe$build\install_diag33.exe" "$PSScriptRoot\install_diag33.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'D33 installer build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-diag33';New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 [PositronDeviceRapi]::CopyFileToDevice("$build\ME3630ComV18D33.dll",'\Windows\ME3630ComV18D33.dll')
 $exe='\Temp\ME3630-install-d33-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\install_diag33.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "D33_INSTALL_PID=$remotePid"
 Start-Sleep -Seconds 2
 if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-diag33-install.txt',"$out\install.txt")){throw 'Install evidence missing'}
 $result=Get-Content "$out\install.txt" -Encoding Unicode -Raw;Write-Output $result
 if($result -notmatch 'result=0 next_attach_only=1 COMPLETE'){throw 'D33 installation failed'}
}finally{[PositronDeviceRapi]::Disconnect()}
