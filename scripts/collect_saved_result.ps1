param([Parameter(Mandatory=$true)][string]$ExpectedStart,[string]$OutDir="$PSScriptRoot\..\evidence\2026-10-11-diag35")
$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 $dest=Join-Path $OutDir 'dial-reset-recovered.txt'
 if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-ras-saved.txt',$dest)){throw 'Saved RAS result missing'}
 $result=Get-Content $dest -Encoding Unicode -Raw
 Write-Output $result
 $start=[regex]::Match($result,'SAVED_START tick=(\d+)').Groups[1].Value
 if($start -ne $ExpectedStart){throw "Unexpected run identity: $start"}
 Write-Output ('RECOVERED_COMPLETE='+($result -match 'SAVED_COMPLETE '))
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-soft-reset.txt',(Join-Path $OutDir 'reset-request.txt'))){Get-Content (Join-Path $OutDir 'reset-request.txt') -Encoding Unicode}
} finally {[PositronDeviceRapi]::Disconnect()}
