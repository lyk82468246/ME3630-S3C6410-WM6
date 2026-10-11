param([switch]$PullD34)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-network-probe'
try {
 [PositronDeviceRapi]::Connect()
 if($PullD34){
  $binary=Join-Path $PSScriptRoot '..\evidence\2026-10-11-diag34\deployed-first.dll'
  if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Windows\ME3630ComV18D34.dll',$binary)){throw 'Deployed D34 file unavailable'}
  Get-FileHash $binary -Algorithm SHA256
 }
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-com-v18.txt',"$out\driver-local.txt")){Get-Content "$out\driver-local.txt" -Encoding Unicode -Tail 65}
} finally {[PositronDeviceRapi]::Disconnect()}
