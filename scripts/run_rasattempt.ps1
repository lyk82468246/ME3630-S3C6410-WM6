param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$stem='ras_attempt';$deviceLog='\Temp\ME3630-ras-attempt.txt';$evidenceName='2026-10-11-ras-attempt';$expected=23;$extraFlags=@()
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ @extraFlags "/I$sdk\Include\Armv4i" "/Fo$build\$stem.obj" "/Fe$build\$stem.exe" "$PSScriptRoot\ras_attempt.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'RAS attempt build failed'}
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
  $prior=[regex]::Match((Get-Content "$out\previous.txt" -Encoding Unicode -Raw),'RAS_ATTEMPT_START a=(\d+)').Groups[1].Value
 }
 $exe='\Temp\ME3630-ras-attempt-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\$stem.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "TEST_PID=$remotePid"
 $deadline=[DateTime]::UtcNow.AddSeconds(75);$finished=$false
 do {
  Start-Sleep -Seconds 3
  if([PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\test.txt")) {
   $snapshot=Get-Content "$out\test.txt" -Encoding Unicode -Raw
   $snapshotStart=[regex]::Match($snapshot,'RAS_ATTEMPT_START a=(\d+)')
   $finished=$snapshotStart.Success -and ($null -eq $prior -or $snapshotStart.Groups[1].Value -ne $prior) -and $snapshot -match 'RAS_ATTEMPT_COMPLETE a=\d+ '
  }
 }while(!$finished -and [DateTime]::UtcNow -lt $deadline)
 if(![PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\test.txt")){throw 'RAS attempt result missing'}
 $result=Get-Content "$out\test.txt" -Encoding Unicode -Raw;Write-Output $result
 $start=[regex]::Match($result,'RAS_ATTEMPT_START a=(\d+)')
 if(!$start.Success -or ($null -ne $prior -and $start.Groups[1].Value -eq $prior)){throw 'No fresh ras attempt run'}
 if($result -notmatch "RAS_ATTEMPT_COMPLETE a=1 " ){throw 'RAS attempt did not pass'}
} finally {[PositronDeviceRapi]::Disconnect()}
