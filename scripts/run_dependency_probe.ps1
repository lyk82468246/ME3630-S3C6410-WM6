$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw '32-bit PowerShell required'}
$source=Get-Content -LiteralPath 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI source not found'}
Add-Type -TypeDefinition $match.Groups[1].Value
$local=Join-Path $PSScriptRoot '..\evidence\2026-10-11-wmdc\dependency-runtime.txt'
try {
 [PositronDeviceRapi]::Connect()
 [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ce_dependency_probe.exe'),'\Temp\ME3630-dependency-probe.exe')
 $pidRemote=[PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-dependency-probe.exe','\Temp',$null)
 Write-Output "Launched diagnostic PID=$pidRemote"
 for($i=0;$i -lt 20;$i++) {
  Start-Sleep -Milliseconds 250
  if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-dependency-probe.txt',$local)) {
   $text=Get-Content -LiteralPath $local -Raw -Encoding Unicode
   if($text -match 'COMPLETE'){Write-Output $text;return}
  }
 }
 throw 'Diagnostic did not finish within five seconds'
}finally{[PositronDeviceRapi]::Disconnect()}
