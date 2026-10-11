param()
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-network-probe'
try {
 [PositronDeviceRapi]::Connect()
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-com-v18.txt',"$out\driver-local.txt")){Get-Content "$out\driver-local.txt" -Encoding Unicode -Tail 65}
} finally {[PositronDeviceRapi]::Disconnect()}
