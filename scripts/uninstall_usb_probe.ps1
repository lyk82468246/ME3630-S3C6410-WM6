# Explicit rollback: unregister our exact USB client match. DLL/logs retained.
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw '32-bit PowerShell required'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
try {
 [PositronDeviceRapi]::Connect()
 [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\uninstall_usb_probe.exe'),'\Temp\ME3630-uninstall.exe')
 [PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-uninstall.exe','\Temp',$null)
 Start-Sleep -Seconds 1
 $out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-usb-probe\uninstall.txt'
 if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-uninstall.txt',$out)){throw 'Uninstall result missing'}
 Get-Content $out -Encoding Unicode
}finally{[PositronDeviceRapi]::Disconnect()}
