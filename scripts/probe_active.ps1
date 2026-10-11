param([string]$OutDir="$PSScriptRoot\..\evidence\2026-10-11-com-v10\inventory",[switch]$UsbState)
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$source=Get-Content "$PSScriptRoot\wm6-probe.ps1" -Raw
$match=[regex]::Match($source,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'Read-only registry helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 $rows=@([WmRead]::Reg('Drivers\Active',1))
 $rows | Set-Content "$OutDir\registry.txt" -Encoding UTF8
 $blocks=($rows -join "`n") -split '(?=\[HKLM\\Drivers\\Active\\)'
 $modem=@($blocks | Where-Object {$_ -match '(?i)ME3630ComV\d+|ME3630Stream'})
 Write-Output "ACTIVE_ME3630_PORT_BLOCKS=$($modem.Count)"
 $modem | Write-Output
 if($UsbState) {
  $usb=@([WmRead]::Reg('Drivers\USB',5))
  $usb | Set-Content "$OutDir\usb-registry-local.txt" -Encoding UTF8
  $active=($rows -join "`n") -split '(?=\[HKLM\\Drivers\\Active\\)'
  $active | Where-Object {$_ -match '(?i)s3c6410_ohci|HCD1:|USBD\.DLL'} | Write-Output
  $usb | Where-Object {$_ -match '(?i)ME3630|6610_5170|DLL|open='} | Write-Output
  $copied=[PositronDeviceRapi]::TryCopyFileFromDevice('\USBDeviceInfo.txt',"$OutDir\USBDeviceInfo.txt")
  Write-Output "USB_INFO_COPIED=$copied"
 }
 Write-Output 'Read-only Active registry snapshot saved locally; no Comm/SIM inventory collected.'
} finally {[PositronDeviceRapi]::Disconnect()}
