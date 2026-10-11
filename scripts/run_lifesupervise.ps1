param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ /DCOM_DIAG38 /DCOM_DIAG39 "/I$sdk\Include\Armv4i" "/Fo$build\life_supervise_d39.obj" "/Fe$build\life_supervise_d39.exe" "$PSScriptRoot\life_supervise.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if($LASTEXITCODE -ne 0){throw 'Lifecycle supervisor build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
. "$PSScriptRoot\rapi_snapshot.ps1"
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-diag39/supervised'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect();$prior=$null;$remote='\Temp\ME3630-supervise-d39.txt'
 if([PositronDeviceRapi]::TryCopyFileFromDevice($remote,"$out\previous-local.txt")){$prior=[regex]::Match((Get-Content "$out\previous-local.txt" -Encoding Unicode -Raw),'SUPERVISE_START tick=(\d+)').Groups[1].Value}
 [PositronDeviceRapi]::CopyFileToDevice("$build\life_capture_d39.exe",'\Temp\ME3630-supervised-capture-d39.exe')
 [PositronDeviceRapi]::CopyFileToDevice("$build\ras_saved.exe",'\Temp\ME3630-supervised-ras-d39.exe')
 $exe='\Temp\ME3630-life-supervise-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\life_supervise_d39.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "SUPERVISE_PID=$remotePid"
 $deadline=[DateTime]::UtcNow.AddSeconds(135);$complete=$false
 do{
  Start-Sleep -Seconds 2
  foreach($item in @(@($remote,'supervise.txt'),@('\Temp\ME3630-life-capture-d39.txt','capture.txt'),@('\Temp\ME3630-ras-saved.txt','dial.txt'))){
   try{[void](Save-WmSnapshot $item[0] (Join-Path $out $item[1]))}
   catch [System.Management.Automation.MethodInvocationException]{if($_.Exception.InnerException -isnot [System.IO.IOException]){throw}}
  }
  if(Test-Path "$out\supervise.txt"){$result=Get-Content "$out\supervise.txt" -Encoding Unicode -Raw;$start=[regex]::Match($result,'SUPERVISE_START tick=(\d+)');$complete=$start.Success -and ($null -eq $prior -or $start.Groups[1].Value -ne $prior) -and $result -match 'SUPERVISE_COMPLETE '}
 }while(!$complete -and [DateTime]::UtcNow -lt $deadline)
 Write-Output $result
 if(!$complete){throw 'No fresh complete supervisor result'}
 if($result -notmatch 'CHILD name=capture .*wait=0 exit_query=1 exit_code=00000000' -or $result -notmatch 'CHILD name=dial .*wait=0 exit_query=1 exit_code=00000000'){throw 'Child did not exit normally; preserve partial results'}
 # Re-read closed files after both children exit. A supervisor completion is
 # not itself evidence of complete capture, actual RasDial, or PPP success.
 foreach($item in @(@('\Temp\ME3630-life-capture-d39.txt','capture.txt','LIFE_CAPTURE COMPLETE'),@('\Temp\ME3630-ras-saved.txt','dial.txt','SAVED_COMPLETE'))){
  if(!(Save-WmSnapshot $item[0] (Join-Path $out $item[1]))){throw 'Closed child result could not be copied'}
  $childResult=Get-Content (Join-Path $out $item[1]) -Encoding Unicode -Raw
  $childStart=[regex]::Match($childResult,'(?:LIFE_CAPTURE start_tick|SAVED_START tick)=(\d+)')
  if(!$childStart.Success -or [uint32]$childStart.Groups[1].Value -lt [uint32]$start.Groups[1].Value -or $childResult -notmatch $item[2]){throw 'No fresh complete child result'}
 }
}finally{[PositronDeviceRapi]::Disconnect()}
