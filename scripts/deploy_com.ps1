param([switch]$PullOnly,[switch]$Test,[switch]$Version5,[switch]$Version6,[switch]$Version7,[switch]$Version8,[switch]$Version9,[switch]$Version10)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$version=4;if($Version5){$version=5}
if($Version6){if($Version5){throw 'Select a single version'};$version=6}
if($Version7){if($Version5 -or $Version6){throw 'Select a single version'};$version=7}
if($Version8){if($Version5 -or $Version6 -or $Version7){throw 'Select a single version'};$version=8}
if($Version9){if($Version5 -or $Version6 -or $Version7 -or $Version8){throw 'Select a single version'};$version=9}
if($Version10){if($Version5 -or $Version6 -or $Version7 -or $Version8 -or $Version9){throw 'Select a single version'};$version=10}
$out=Join-Path $PSScriptRoot "..\evidence\2026-10-11-com-v$version"
New-Item -ItemType Directory -Path $out -Force | Out-Null
$testLog='\Temp\ME3630-com-test.txt';if($Version5){$testLog='\Temp\ME3630-com-test-v5.txt'}
if($Version6){$testLog='\Temp\ME3630-com-test-v6.txt'}
if($Version7){$testLog='\Temp\ME3630-com-test-v7.txt'}
if($Version8){$testLog='\Temp\ME3630-com-test-v8.txt'}
if($Version9){$testLog='\Temp\ME3630-com-test-v9.txt'}
if($Version10){$testLog='\Temp\ME3630-com-test-v10.txt'}
$previousStart=$null
try {
 [PositronDeviceRapi]::Connect()
 if($Test) {
  if($Version7 -or $Version8 -or $Version9 -or $Version10) {
   # Registration does not replace an already active V6 instance. Wait for an
   # actual V7 activation marker, not merely a successful installer return.
   $readyDeadline=[DateTime]::UtcNow.AddSeconds(40);$ready=$false
   do {
    if([PositronDeviceRapi]::TryCopyFileFromDevice("\Temp\ME3630-com-v$version.txt","$out\driver.txt")) {
     $ready=(Get-Content "$out\driver.txt" -Encoding Unicode -Raw) -match "V$version READY"
    }
    if(!$ready){Start-Sleep -Seconds 2}
   } while(!$ready -and [DateTime]::UtcNow -lt $readyDeadline)
   if(!$ready){throw "V$version has not activated yet; no test launched"}
  }
  if([PositronDeviceRapi]::TryCopyFileFromDevice($testLog,"$out\test-raw.txt")) {
   $prior=[regex]::Match((Get-Content "$out\test-raw.txt" -Encoding Unicode -Raw),'start_tick=(\d+)')
   if($prior.Success){$previousStart=$prior.Groups[1].Value}
  }
  $exe='\Temp\ME3630-com-test-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot "..\build\com_test_v$version.exe"),$exe)
  $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "TEST_PID=$remotePid"
  Start-Sleep -Seconds 50
 } elseif(!$PullOnly) {
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot "..\build\ME3630ComV$version.dll"),"\Windows\ME3630ComV$version.dll")
  $exe='\Temp\ME3630-install-v4-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot "..\build\install_com_v$version.exe"),$exe)
  $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "INSTALL_PID=$remotePid"
  Start-Sleep -Seconds 2
  if(![PositronDeviceRapi]::TryCopyFileFromDevice("\Temp\ME3630-install-v$version.txt","$out\install.txt")){throw 'Install result missing'}
  $result=Get-Content "$out\install.txt" -Encoding Unicode -Raw;Write-Output $result
  if($result -notmatch 'installed=1'){throw "V$version install failed"}
 }
 if([PositronDeviceRapi]::TryCopyFileFromDevice("\Temp\ME3630-com-v$version.txt","$out\driver.txt")){Get-Content "$out\driver.txt" -Encoding Unicode -Tail 50}
 if([PositronDeviceRapi]::TryCopyFileFromDevice($testLog,"$out\test-raw.txt")) {
  $redacted=(Get-Content "$out\test-raw.txt" -Encoding Unicode -Raw) -replace '(?im)(IMEI\s*:\s*)[0-9]+','$1[REDACTED]'
  $redacted | Set-Content "$out\test.txt" -Encoding Unicode;Write-Output $redacted
  if($Test) {
   $current=[regex]::Match($redacted,'start_tick=(\d+)')
   if(!$current.Success -or ($null -ne $previousStart -and $current.Groups[1].Value -eq $previousStart)){throw 'No fresh test start; refusing to accept old results'}
  }
  $expectedChecks=32;if($Version7){$expectedChecks=44}
  if($Version8){$expectedChecks=68}
  if($Version9){$expectedChecks=102}
  if($Version10){$expectedChecks=102}
  if($Test -and $redacted -notmatch "COMMANDS_PASSED=4/4 CONTROL_CHECKS=$expectedChecks FAILURES=0 COMPLETE"){throw "V$version test did not pass all checks"}
 } elseif($Test){throw 'Test result missing'}
} finally {[PositronDeviceRapi]::Disconnect()}
