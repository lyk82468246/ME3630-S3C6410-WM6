param([switch]$Remove,[switch]$PullOnly,[string]$OutDir)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
if(!$PullOnly) {
 & $cl @flags "/Fo$build\me3630_watch.obj" "/Fe$build\ME3630UsbWatch.dll" "$PSScriptRoot\me3630_watch.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_watch.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'Watch DLL build failed'}
 & $cl @flags "/Fo$build\install_watch.obj" "/Fe$build\install_watch.exe" "$PSScriptRoot\install_watch.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'Watch installer build failed'}
 & $cl @flags /DWATCH_REMOVE "/Fo$build\uninstall_watch.obj" "/Fe$build\uninstall_watch.exe" "$PSScriptRoot\install_watch.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'Watch uninstaller build failed'}
}
if([Environment]::Is64BitProcess){throw 'Run using SysWOW64 Windows PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
if([string]::IsNullOrWhiteSpace($OutDir)){$out=Join-Path $PSScriptRoot "..\evidence\2026-11-16-usb-watch"}else{$out=[IO.Path]::GetFullPath($OutDir)}
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 if(!$PullOnly) {
  [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'ME3630UsbWatch.dll'),'\Windows\ME3630UsbWatch.dll')
  if($Remove) {
   [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'uninstall_watch.exe'),'\Temp\watch-uninstall.exe')
   $remotePid=[PositronDeviceRapi]::LaunchProcess('\Temp\watch-uninstall.exe','\Temp',$null)
   $remoteResult='\Temp\ME3630-watch-uninstall.txt'
  } else {
   [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'install_watch.exe'),'\Temp\watch-install.exe')
   $remotePid=[PositronDeviceRapi]::LaunchProcess('\Temp\watch-install.exe','\Temp',$null)
   $remoteResult='\Temp\ME3630-watch-install.txt'
  }
  Write-Output "WATCH_PID=$remotePid"
  $finished=$false
  for($i=0;$i -lt 20;$i++) {
   Start-Sleep -Milliseconds 250
   if([PositronDeviceRapi]::TryCopyFileFromDevice($remoteResult,"$out\install.txt")) {
    $text=Get-Content "$out\install.txt" -Encoding Unicode -Raw
    if($text -match 'COMPLETE'){Write-Output $text;$finished=$true;break}
   }
  }
  if(!$finished){throw 'Watch installer did not finish'}
 }
 if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-usb-watch.txt',"$out\watch.txt")) {
  Get-Content "$out\watch.txt" -Encoding Unicode
 } else {Write-Output 'watch log not present yet'}
}finally{[PositronDeviceRapi]::Disconnect()}
