param([switch]$PullOnly)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-stability'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 if(!$PullOnly) {
  $exe='\Temp\ME3630-stability-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\serial_stability.exe'),$exe)
  $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "MONITOR_PID=$remotePid"
 }
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-stability.txt',"$out\monitor.txt")){Get-Content "$out\monitor.txt" -Encoding Unicode -Tail 35}
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-com-v5.txt',"$out\driver-v5.txt")){Write-Output 'Driver trace saved'}
} finally {[PositronDeviceRapi]::Disconnect()}
