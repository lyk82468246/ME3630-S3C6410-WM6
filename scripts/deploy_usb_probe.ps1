param([switch]$PullOnly,[switch]$Version2)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Run using SysWOW64 Windows PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-usb-probe'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 if(!$PullOnly) {
  if($Version2) {
   [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ME3630ProbeV2.dll'),'\Windows\ME3630ProbeV2.dll')
   [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\install_usb_probe_v2.exe'),'\Temp\ME3630-install-v2.exe')
   $remotePid=[PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-install-v2.exe','\Temp',$null)
   $remoteResult='\Temp\ME3630-install-v2.txt'
  } else {
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ME3630Probe.dll'),'\Windows\ME3630Probe.dll')
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\install_usb_probe.exe'),'\Temp\ME3630-install.exe')
  $remotePid=[PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-install.exe','\Temp',$null)
  $remoteResult='\Temp\ME3630-install.txt'
  }
  Write-Output "INSTALL_PID=$remotePid"
  $finished=$false
  for($i=0;$i -lt 20;$i++) {
   Start-Sleep -Milliseconds 250
   if([PositronDeviceRapi]::TryCopyFileFromDevice($remoteResult,"$out\install.txt")) {
    $text=Get-Content "$out\install.txt" -Encoding Unicode -Raw
    if($text -match 'COMPLETE') {Write-Output $text;$finished=$true;break}
   }
  }
  if(!$finished -or $text -notmatch 'installed=1'){throw 'Installer did not confirm success'}
 }
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-usb-probe.txt',"$out\usb-probe.txt")) {
  Get-Content "$out\usb-probe.txt" -Encoding Unicode
 }
}finally{[PositronDeviceRapi]::Disconnect()}
