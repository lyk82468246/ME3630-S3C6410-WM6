param([switch]$Watch,[switch]$Unit,[switch]$BuildOnly)
$ErrorActionPreference='Stop'
if($Watch -and $Unit){throw 'Choose observation or isolated unit test'}
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build';$program=if($Unit){'life_unit'}else{'life_capture'}
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ "/I$sdk\Include\Armv4i" "/Fo$build\$program.obj" "/Fe$build\$program.exe" "$PSScriptRoot\$program.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Lifecycle program build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-diag37';New-Item -ItemType Directory -Path $out -Force | Out-Null
$deviceLog=if($Unit){'\Temp\ME3630-life-unit.txt'}else{'\Temp\ME3630-life-capture.txt'}
$target=if($Unit){'life-unit-second.txt'}elseif($Watch){'capture-watch.txt'}else{'capture.txt'}
try {
 [PositronDeviceRapi]::Connect();$prior=$null
 if([PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\capture-previous-local.txt")){$prior=[regex]::Match((Get-Content "$out\capture-previous-local.txt" -Encoding Unicode -Raw),'start_tick=(\d+)').Groups[1].Value}
 $exe='\Temp\ME3630-'+$program+'-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\$program.exe",$exe)
 $argsText=if($Watch){'--watch'}else{$null}
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$argsText);Write-Output "LIFE_PID=$remotePid WATCH=$Watch"
 $deadline=[DateTime]::UtcNow.AddSeconds($(if($Watch){110}else{15}));$finished=$false
 do {
  Start-Sleep -Seconds 2;$copied=$false
  try {$copied=[PositronDeviceRapi]::TryCopyFileFromDevice($deviceLog,"$out\$target")}
  catch [System.Management.Automation.MethodInvocationException] {if($_.Exception.InnerException -isnot [System.IO.IOException]){throw}}
  if($copied){$result=Get-Content "$out\$target" -Encoding Unicode -Raw;$start=[regex]::Match($result,'start_tick=(\d+)');$finished=$start.Success -and ($null -eq $prior -or $start.Groups[1].Value -ne $prior) -and $result -match '(LIFE_CAPTURE COMPLETE|LIFE_UNIT CHECKS=\d+ FAILURES=\d+ COMPLETE)'}
 }while(!$finished -and [DateTime]::UtcNow -lt $deadline)
 Write-Output $result
 if(!$finished){throw 'No fresh complete lifecycle result within collection window'}
 if($Unit -and $result -notmatch 'LIFE_UNIT CHECKS=15 FAILURES=0 COMPLETE'){throw 'Lifecycle tests failed'}
}finally{[PositronDeviceRapi]::Disconnect()}
