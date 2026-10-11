param([string]$EvidenceDir="$PSScriptRoot\..\evidence\2026-10-11-stability")
$ErrorActionPreference='Stop'
$monitor=Get-Content -LiteralPath "$EvidenceDir\monitor.txt" -Encoding Unicode -Raw
$start=[regex]::Match($monitor,'START tick=(\d+)')
$complete=[regex]::Match($monitor,'COMPLETE elapsed=(\d+)')
if(!$start.Success -or !$complete.Success){throw 'Completed monitor required'}
$begin=[uint64]$start.Groups[1].Value
$end=$begin+[uint64]$complete.Groups[1].Value
$trace=Get-Content -LiteralPath "$EvidenceDir\driver-v5.txt" -Encoding Unicode
$selected=New-Object 'System.Collections.Generic.List[string]'
$keep=$false
foreach($line in $trace){
 $m=[regex]::Match($line,'^\[tick=(\d+)\]')
 if($m.Success){$tick=[uint64]$m.Groups[1].Value;$keep=$tick -ge $begin -and $tick -le $end}
 if($keep){$selected.Add($line)}
}
$redacted=($selected -join "`r`n") -replace '(?im)(IMEI:\s*)\d+','$1REDACTED'
Set-Content -LiteralPath "$EvidenceDir\driver-window.txt" -Value $redacted -Encoding Unicode
Write-Output "WINDOW=$begin..$end LINES=$($selected.Count)"
Write-Output $complete.Value
