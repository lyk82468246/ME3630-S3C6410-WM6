# RAPI can return false after writing a partial local file. Do not replace the
# last successfully copied evidence until a complete copy operation succeeds.
function Save-WmSnapshot([string]$RemotePath,[string]$LocalPath) {
 $stage=$LocalPath+'-partial-local.txt'
 if(![PositronDeviceRapi]::TryCopyFileFromDevice($RemotePath,$stage)){return $false}
 Move-Item -LiteralPath $stage -Destination $LocalPath -Force
 return $true
}
