param([switch]$Dial,[switch]$BuildOnly)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ "/I$sdk\Include\Armv4i" "/Fo$build\ras_saved.obj" "/Fe$build\ras_saved.exe" "$PSScriptRoot\ras_saved.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Saved RAS tool build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-ras-saved'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$target=if($Dial){'dial.txt'}else{'inspect.txt'}
$deviceLog='\Temp\ME3630-ras-saved.txt'
try {
 [PositronDeviceRapi]::Connect()
 $prior=$null
 if([PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\previous-local.txt")){$prior=[regex]::Match((Get-Content "$out\previous-local.txt" -Encoding Unicode -Raw),'SAVED_START tick=(\d+)').Groups[1].Value}
 $exe='\Temp\ME3630-ras-saved-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\ras_saved.exe",$exe)
 $argsText=if($Dial){'--dial'}else{$null}
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$argsText);Write-Output "SAVED_PID=$remotePid"
 $deadline=[DateTime]::UtcNow.AddSeconds(65);$finished=$false
 do {
  Start-Sleep -Seconds 2
  $copied=$false
  try {$copied=[PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\$target")}
  catch [System.Management.Automation.MethodInvocationException] {
   if($_.Exception.InnerException -isnot [System.IO.IOException]){throw}
   Write-Warning 'Log snapshot temporarily unavailable; retrying within the existing collection deadline.'
  }
  if($copied){
   $result=Get-Content "$out\$target" -Encoding Unicode -Raw
   $start=[regex]::Match($result,'SAVED_START tick=(\d+)')
   $finished=$start.Success -and ($null -eq $prior -or $start.Groups[1].Value -ne $prior) -and $result -match 'SAVED_COMPLETE '
  }
 }while(!$finished -and [DateTime]::UtcNow -lt $deadline)
 Write-Output $result
 if(!$finished){throw 'Saved-entry check did not complete within collection window'}
} finally {[PositronDeviceRapi]::Disconnect()}
