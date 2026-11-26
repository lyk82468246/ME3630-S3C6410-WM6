param(
    [string]$OutLog = ''
)
$ErrorActionPreference = 'Stop'

$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC'
$ide = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE'
$winSdk = 'C:\Program Files\Microsoft SDKs\Windows\v6.0A'
if (!(Test-Path $winSdk)) { $winSdk = 'C:\Program Files (x86)\Microsoft SDKs\Windows\v6.0A' }

$env:PATH = "$vcRoot\bin;$ide;$winSdk\Bin;" + $env:PATH
$env:INCLUDE = "$vcRoot\include;$winSdk\Include"
$env:LIB = "$vcRoot\lib;$winSdk\Lib"

$build = Join-Path $PSScriptRoot '..\build'
$tmp = Join-Path $build 'host_test_local'
New-Item -ItemType Directory -Path $tmp -Force | Out-Null

$exe = Join-Path $tmp 'test_d41_host.exe'
$obj = Join-Path $tmp 'test_d41_host.obj'

& "$vcRoot\bin\cl.exe" /nologo /TC /MT /W3 /O2 "/I$PSScriptRoot" "/Fo$obj" "/Fe$exe" (Join-Path $PSScriptRoot 'test_d41_host.c') /link /DYNAMICBASE /NXCOMPAT kernel32.lib
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }

$output = & $exe
$rc = $LASTEXITCODE
if (-not [string]::IsNullOrWhiteSpace($OutLog)) {
    $dir = Split-Path -Parent $OutLog
    if ($dir) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $output | Set-Content -Path $OutLog -Encoding UTF8
}
$output
if ($rc -ne 0) { throw "Host test failed with exit code $rc" }
