param(
    [switch]$BuildOnly,
    [switch]$LoadTest,
    [string]$OutDir = ''
)
$ErrorActionPreference = 'Stop'

$env:PATH = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;' + $env:PATH
$sdk = 'C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build = Join-Path $PSScriptRoot '..\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null

$flags = @('/nologo', '/TC', '/O1', '/GS-', '/DUNICODE', '/D_UNICODE', '/DUNDER_CE', '/D_WIN32_WCE=0x502', '/DARM', '/D_ARM_', "/I$sdk\Include\Armv4i")
& $cl @flags "/Fo$build\rndis_dep_probe.obj" "/Fe$build\rndis_dep_probe.exe" (Join-Path $PSScriptRoot 'rndis_dep_probe.c') /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if ($LASTEXITCODE -ne 0) { throw 'rndis_dep_probe build failed' }

if ($BuildOnly) {
    Write-Output "BUILT $(Join-Path $build 'rndis_dep_probe.exe')"
    exit 0
}

if ([Environment]::Is64BitProcess) { throw 'Run using SysWOW64 Windows PowerShell' }
$source = Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match = [regex]::Match($source, "(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if (!$match.Success) { throw 'RAPI helper missing' }
Add-Type -TypeDefinition $match.Groups[1].Value

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $out = Join-Path $PSScriptRoot '..\evidence\2026-11-25-rndis-dep-probe'
} else {
    $out = [IO.Path]::GetFullPath($OutDir)
}
New-Item -ItemType Directory -Path $out -Force | Out-Null

try {
    [PositronDeviceRapi]::Connect()
    [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'rndis_dep_probe.exe'), '\Temp\ME3630-rndis-dep-probe.exe')
    $args = ''
    if ($LoadTest) {
        $rndismp = Join-Path $build 'rndismp.dll'
        $usb8023 = Join-Path $build 'usb8023.dll'
        if (!(Test-Path $rndismp) -or !(Test-Path $usb8023)) {
            throw 'Run scripts\build_rndis_ce5.ps1 first to generate build\rndismp.dll and build\usb8023.dll'
        }
        [PositronDeviceRapi]::CopyFileToDevice($rndismp, '\Temp\rndismp.dll')
        [PositronDeviceRapi]::CopyFileToDevice($usb8023, '\Temp\usb8023.dll')
        $args = 'loadtest'
    }
    $remotePid = [PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-rndis-dep-probe.exe', $args, $null)
    Write-Output "RNDIS_DEP_PROBE_PID=$remotePid"
    $finished = $false
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 250
        if ([PositronDeviceRapi]::TryCopyFileFromDevice('\Temp\ME3630-rndis-dep-probe.txt', "$out\result.txt")) {
            $text = Get-Content "$out\result.txt" -Encoding Unicode -Raw
            if ($text -match 'COMPLETE') {
                Write-Output $text
                $finished = $true
                break
            }
        }
    }
    if (!$finished) { throw 'rndis_dep_probe did not complete within 10s' }
} finally {
    [PositronDeviceRapi]::Disconnect()
}
