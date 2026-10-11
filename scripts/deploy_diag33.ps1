param([switch]$BuildOnly,[switch]$Version34,[switch]$Revision2,[switch]$Version35,[switch]$Version36)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
$diag=if($Version34){34}else{33}
if($Revision2){if(!$Version34){throw 'Revision2 requires Version34'};$diag='34R2'}
if($Version36){if($Version34 -or $Revision2 -or $Version35){throw 'Select Version36 alone'};$Version35=$true}
if($Version35){if($Version34 -or $Revision2){throw 'Select Version35 alone'};$Version34=$true;$Revision2=$true;$diag=35}
if($Version36){$diag=36}
$flags=@('/nologo','/TC','/O1','/GS-','/DUNICODE','/D_UNICODE','/DUNDER_CE','/D_WIN32_WCE=0x502','/DARM','/D_ARM_',"/I$sdk\Include\Armv4i")
$driverFlags=$flags+@('/DSTREAM_V3','/DCOM_V4','/DCOM_DIAG33');for($n=5;$n -le 18;$n++){$driverFlags+="/DCOM_V$n"}
if($Version34){$driverFlags+='/DCOM_DIAG34';$flags+='/DCOM_DIAG34'}
if($Revision2){$driverFlags+='/DCOM_DIAG34R2';$flags+='/DCOM_DIAG34R2'}
if($Version35){$driverFlags+='/DCOM_DIAG35';$flags+='/DCOM_DIAG35'}
if($Version36){$driverFlags+='/DCOM_DIAG36';$flags+='/DCOM_DIAG36'}
& $cl @driverFlags "/Fo$build\me3630_diag$diag.obj" "/Fe$build\ME3630ComV18D$diag.dll" "$PSScriptRoot\me3630_probe.c" /link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWSCE,5.02 "/DEF:$PSScriptRoot\me3630_com.def" "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'D33 driver build failed'}
& $cl @flags "/Fo$build\install_diag$diag.obj" "/Fe$build\install_diag$diag.exe" "$PSScriptRoot\install_diag33.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'D33 installer build failed'}
if($Version34){
 & $cl @flags "/Fo$build\open_unit.obj" "/Fe$build\open_unit.exe" "$PSScriptRoot\open_unit.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'Open unit build failed'}
}
if($Version35){
 $compatFlags=$flags+@('/DCOM_V9','/DCOM_V15','/DCOM_V18')
 & $cl @compatFlags "/Fo$build\compat_unit.obj" "/Fe$build\compat_unit.exe" "$PSScriptRoot\compat_unit.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
 if($LASTEXITCODE -ne 0){throw 'Compatibility unit build failed'}
}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot "..\evidence\2026-10-11-diag$diag";New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect()
 if($Version34){
  $priorUnit=$null
  if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-open-unit.txt',"$out\unit-previous-local.txt")){$priorUnit=[regex]::Match((Get-Content "$out\unit-previous-local.txt" -Encoding Unicode -Raw),'start_tick=(\d+)').Groups[1].Value}
  $unitExe='\Temp\ME3630-open-unit-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice("$build\open_unit.exe",$unitExe)
  $unitPid=[PositronDeviceRapi]::LaunchProcess($unitExe,'\Temp',$null);Write-Output "UNIT_PID=$unitPid"
  Start-Sleep -Seconds 2
  if(![PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-open-unit.txt',"$out\open-unit.txt")){throw 'Open unit evidence missing'}
  $unit=Get-Content "$out\open-unit.txt" -Encoding Unicode -Raw;Write-Output $unit
  $unitStart=[regex]::Match($unit,'start_tick=(\d+)')
  if(!$unitStart.Success -or ($null -ne $priorUnit -and $unitStart.Groups[1].Value -eq $priorUnit)){throw 'No fresh ownership unit run'}
  if($unit -notmatch 'OPEN_UNIT CHECKS=15 FAILURES=0 COMPLETE'){throw 'Open ownership test failed; no driver installed'}
 }
 if($Version35){
  $priorCompat=$null
  if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-compat-unit.txt',"$out\compat-previous-local.txt")){$priorCompat=[regex]::Match((Get-Content "$out\compat-previous-local.txt" -Encoding Unicode -Raw),'start_tick=(\d+)').Groups[1].Value}
  $compatExe='\Temp\ME3630-compat-unit-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
  [PositronDeviceRapi]::CopyFileToDevice("$build\compat_unit.exe",$compatExe)
  $compatPid=[PositronDeviceRapi]::LaunchProcess($compatExe,'\Temp',$null);Write-Output "COMPAT_PID=$compatPid"
  $deadline=[DateTime]::UtcNow.AddSeconds(15);$complete=$false
  do {
   Start-Sleep -Seconds 1
   if([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-compat-unit.txt',"$out\compat-unit.txt")){
    $compat=Get-Content "$out\compat-unit.txt" -Encoding Unicode -Raw
    $compatStart=[regex]::Match($compat,'start_tick=(\d+)')
    $complete=$compatStart.Success -and ($null -eq $priorCompat -or $compatStart.Groups[1].Value -ne $priorCompat) -and $compat -match 'COMPAT_UNIT CHECKS=\d+ FAILURES=\d+ COMPLETE'
   }
  }while(!$complete -and [DateTime]::UtcNow -lt $deadline)
  Write-Output $compat
  $expectedChecks=if($Version36){33}else{26}
  if(!$complete -or $compat -notmatch "COMPAT_UNIT CHECKS=$expectedChecks FAILURES=0 COMPLETE"){throw 'Compatibility test failed; no driver installed'}
 }
 [PositronDeviceRapi]::CopyFileToDevice("$build\ME3630ComV18D$diag.dll","\Windows\ME3630ComV18D$diag.dll")
 $exe="\Temp\ME3630-install-d$diag-"+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\install_diag$diag.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "D${diag}_INSTALL_PID=$remotePid"
 Start-Sleep -Seconds 2
 if(![PositronDeviceRapi]::TryCopyFileFromDevice("\Temp\ME3630-diag$diag-install.txt","$out\install.txt")){throw 'Install evidence missing'}
 $result=Get-Content "$out\install.txt" -Encoding Unicode -Raw;Write-Output $result
 if($result -notmatch 'result=0 next_attach_only=1 COMPLETE'){throw 'D33 installation failed'}
}finally{[PositronDeviceRapi]::Disconnect()}
