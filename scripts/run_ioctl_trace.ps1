$ErrorActionPreference='Stop'
if([Environment]::Is64BitProcess){throw 'Use 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-com-v4'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ME3630IoctlTrace.dll'),'\Windows\ME3630IoctlTrace.dll')
 $exe='\Temp\ME3630-ioctl-trace-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice((Join-Path $PSScriptRoot '..\build\ioctl_trace.exe'),$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null)
 Write-Output "TRACE_PID=$remotePid"
 Start-Sleep -Seconds 3
 if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-ioctl-abi.txt',"$out\ioctl-abi.txt")){throw 'Trace missing'}
 Get-Content "$out\ioctl-abi.txt" -Encoding Unicode
} finally {[PositronDeviceRapi]::Disconnect()}
