param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;'+$env:PATH
$sdk='C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl='C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build=Join-Path $PSScriptRoot '..\build'
& $cl /nologo /TC /O1 /GS- /DUNICODE /D_UNICODE /DUNDER_CE /D_WIN32_WCE=0x502 /DARM /D_ARM_ /DCOM_DIAG38 /DCOM_DIAG39 "/I$sdk\Include\Armv4i" "/Fo$build\life_snapshot_d39.obj" "/Fe$build\life_snapshot_d39.exe" "$PSScriptRoot\life_snapshot.c" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib toolhelp.lib
if($LASTEXITCODE -ne 0){throw 'Lifecycle snapshot build failed'}
if($BuildOnly){return}
if([Environment]::Is64BitProcess){throw 'Use Joe 32-bit PowerShell'}
$source=Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if(!$match.Success){throw 'RAPI helper missing'}
Add-Type -TypeDefinition $match.Groups[1].Value
. "$PSScriptRoot\rapi_snapshot.ps1"
$out=Join-Path $PSScriptRoot '..\evidence\2026-10-11-diag39'
New-Item -ItemType Directory -Path $out -Force | Out-Null
try {
 [PositronDeviceRapi]::Connect();$prior=$null
 $remote='\Temp\ME3630-life-snapshot-d39.txt'
 if([PositronDeviceRapi]::TryCopyFileFromDevice($remote,"$out\snapshot-previous-local.txt")){$prior=[regex]::Match((Get-Content "$out\snapshot-previous-local.txt" -Encoding Unicode -Raw),'SNAPSHOT_START tick=(\d+)').Groups[1].Value}
 $exe='\Temp\ME3630-life-snapshot-'+[DateTime]::Now.ToString('HHmmss')+'.exe'
 [PositronDeviceRapi]::CopyFileToDevice("$build\life_snapshot_d39.exe",$exe)
 $remotePid=[PositronDeviceRapi]::LaunchProcess($exe,'\Temp',$null);Write-Output "SNAPSHOT_PID=$remotePid"
 $deadline=[DateTime]::UtcNow.AddSeconds(15);$complete=$false
 do {
  Start-Sleep -Seconds 1
  if(Save-WmSnapshot $remote "$out\snapshot.txt"){
   $result=Get-Content "$out\snapshot.txt" -Encoding Unicode -Raw;$start=[regex]::Match($result,'SNAPSHOT_START tick=(\d+)')
   $complete=$start.Success -and ($null -eq $prior -or $start.Groups[1].Value -ne $prior) -and $result -match 'SNAPSHOT_COMPLETE '
  }
 }while(!$complete -and [DateTime]::UtcNow -lt $deadline)
 Write-Output ($result -split "`r?`n" | Where-Object {$_ -notmatch '^seq='})
 if(!$complete){throw 'No fresh complete snapshot'}
}finally{[PositronDeviceRapi]::Disconnect()}
