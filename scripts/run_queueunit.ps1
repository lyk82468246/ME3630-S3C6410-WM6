param([switch]$BuildOnly,[switch]$Version15)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$stem='queue_unit';$deviceLog='\Temp\ME3630-queue-unit.txt';$evidenceName='2026-10-11-queue-unit';$expected=10;$extraFlags=@()
if($Version15){$stem='queue_unit_v15';$deviceLog='\Temp\ME3630-queue-unit-v15.txt';$evidenceName='2026-10-11-queue-unit-v15';$expected=13;$extraFlags=@('/DCOM_V15')}
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ @extraFlags "/I$sdk\Include\Armv4i" "/Fo$build\$stem.obj" "/Fe$build\$stem.exe" "$PSScriptRoot\queue_unit.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Queue unit build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot "..\evidence\$evidenceName"
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 $prior=$null
 if([PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\previous.txt")) {
  $prior=[regex]::Match((Get-Content "$out\previous.txt" -Encoding Unicode -Raw),'start_tick=(\d+)').Groups[1].Value
 }
 $exe='\Temp\ME3630-queue-unit-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\$stem.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "TEST_PID=$remotePid"
 Start-Sleep -Seconds 3
 if(![PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\test.txt")){throw 'Queue unit result missing'}
 $result=Get-Content "$out\test.txt" -Encoding Unicode -Raw;Write-Output $result
 $start=[regex]::Match($result,'start_tick=(\d+)')
 if(!$start.Success -or ($null -ne $prior -and $start.Groups[1].Value -eq $prior)){throw 'No fresh queue unit run'}
 if($result -notmatch "QUEUE_UNIT CHECKS=$expected FAILURES=0 COMPLETE"){throw 'Queue unit did not pass'}
} finally {[PositronDeviceRapi]::Disconnect()}
