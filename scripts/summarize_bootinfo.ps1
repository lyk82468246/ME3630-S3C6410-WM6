param([string]$LogPath="$PSScriptRoot\..\evidence\2026-10-11-com-v6\driver.txt")
$ErrorActionPreference='Stop'
$log=Get-Content -LiteralPath $LogPath -Encoding Unicode -Raw
$attachments=[regex]::Matches($log,'\[tick=(\d+)\] ATTACH vid=19d2 pid=1432')
$frames=[regex]::Matches($log,'ADB_BOOTINFO sample=(\d+) tick=(\d+) \[([\s\S]*?)\]\r?\n')
$rows=@()
foreach($frame in $frames) {
 $payload=$frame.Groups[3].Value
 $uptime=[regex]::Match($payload,'(?m)^\s*(\d+\.\d+)\s+\d+\.\d+')
 $boot=[regex]::Match($payload,'(?i)\b[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\b')
 $tick=[uint32]$frame.Groups[2].Value
 $attach=$attachments | Where-Object {[uint32]$_.Groups[1].Value -le $tick} | Select-Object -Last 1
 $rows+=[pscustomobject]@{hostTick=$tick;attachTick=if($attach){[uint32]$attach.Groups[1].Value}else{$null};sample=[int]$frame.Groups[1].Value;uptimeSeconds=if($uptime.Success){[double]::Parse($uptime.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)}else{$null};bootId=if($boot.Success){$boot.Value}else{$null}}
}
$rows | Format-Table -AutoSize
$ids=@($rows | Where-Object bootId | Select-Object -ExpandProperty bootId -Unique)
Write-Output "ATTACHMENTS=$($attachments.Count) BOOTINFO_FRAMES=$($frames.Count) DISTINCT_BOOT_IDS=$($ids.Count)"
if(!$frames.Count){Write-Output 'No boot information: no inference about module restarts is possible.'}
elseif($ids.Count -gt 1){Write-Output 'Different kernel boot IDs observed: Linux kernel reboot occurred; cause is not yet established.'}
else{Write-Output 'Compare samples from distinct attachments before inferring that USB reconnection happened without a Linux reboot.'}
