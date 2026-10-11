# Host-only regression. Fake RAPI writes fixtures; no WMDC/device calls.
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class PositronDeviceRapi {
 public static int Mode;
 public static bool TryCopyFileFromDevice(string remote, string local) {
  File.WriteAllText(local, Mode==0 ? "complete" : "partial");
  if(Mode==2)throw new IOException("simulated read failure");
  return Mode==0;
 }
}
'@
. "$PSScriptRoot\rapi_snapshot.ps1"
$testDir=Join-Path $PSScriptRoot ('..\build\snapshot-unit-local\'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$target=Join-Path $testDir 'evidence.txt'
$checks=0
function Assert-Snapshot([string]$Name,[bool]$Pass){if(!$Pass){throw "FAIL $Name"};$script:checks++;Write-Output "PASS $Name"}
[PositronDeviceRapi]::Mode=0
Assert-Snapshot 'successful_copy_published' ((Save-WmSnapshot '\test.txt' $target) -and (Get-Content $target -Raw) -eq 'complete')
Assert-Snapshot 'successful_stage_retired' (!(Test-Path ($target+'-partial-local.txt')))
[PositronDeviceRapi]::Mode=1
Assert-Snapshot 'false_copy_preserves_evidence' (!(Save-WmSnapshot '\test.txt' $target) -and (Get-Content $target -Raw) -eq 'complete')
[PositronDeviceRapi]::Mode=2
$threw=$false
try{[void](Save-WmSnapshot '\test.txt' $target)}catch{$threw=$true}
Assert-Snapshot 'exception_preserves_evidence' ($threw -and (Get-Content $target -Raw) -eq 'complete')
[PositronDeviceRapi]::Mode=0
Assert-Snapshot 'retry_publishes_success' ((Save-WmSnapshot '\test.txt' $target) -and (Get-Content $target -Raw) -eq 'complete' -and !(Test-Path ($target+'-partial-local.txt')))
Write-Output "RAPI_SNAPSHOT_UNIT CHECKS=$checks FAILURES=0 COMPLETE"
