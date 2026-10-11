param([switch]$PullOnly,[switch]$Test)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Run using SysWOW64 Windows PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-stream-v3'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 if($Test) {
  $testExe='\Temp\ME3630-stream-test-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\stream_test.exe'),$testExe)
  $remotePid=[PositronDeviceRapi]::LaunchProcess($testExe,'\Temp',$null)
  Write-Output "TEST_PID=$remotePid"
  # The native test holds the idle timer awake for 25 seconds before opening.
  # Keep this bounded; an incomplete test is explicitly reported.
  Start-Sleep -Seconds 40
 } elseif(!$PullOnly) {
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ME3630StreamV3.dll'),'\Windows\ME3630StreamV3.dll')
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\install_stream.exe'),'\Temp\ME3630-install-v3.exe')
  $remotePid=[PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-install-v3.exe','\Temp',$null)
  Write-Output "INSTALL_PID=$remotePid"
  Start-Sleep -Seconds 2
  if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-install-v3.txt',"$out\install.txt")){throw 'Installer result missing'}
  $result=Get-Content "$out\install.txt" -Encoding Unicode -Raw
  Write-Output $result
  if($result -notmatch 'installed=1'){throw 'Stream installer failed'}
 }
 foreach($item in @(@('\Temp\ME3630-stream-v3.txt','driver.txt'),@('\Temp\ME3630-stream-test.txt','test-raw.txt'))) {
  if($item[1] -eq 'test-raw.txt') {
   if([PositronDeviceRapi]::TryCopyFileFromDevice($item[0],"$out\test-raw.txt")) {
    $redacted=(Get-Content "$out\test-raw.txt" -Encoding Unicode -Raw) -replace '(?im)(IMEI\s*:\s*)[0-9]+','$1[REDACTED]'
    # Generated evidence: do not publish a subscriber/device identifier.
    $redacted | Set-Content "$out\test.txt" -Encoding Unicode
    Write-Output $redacted
   }
   continue
  }
  if([PositronDeviceRapi]::TryCopyFileFromDevice($item[0],"$out\$($item[1])")){Get-Content "$out\$($item[1])" -Encoding Unicode -Tail 70}
 }
} finally {[PositronDeviceRapi]::Disconnect()}
