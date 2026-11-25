param(
    [switch]$BuildOnly,
    [switch]$Remove,
    [string]$OutDir = ''
)
$ErrorActionPreference = 'Stop'

$env:PATH = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;' + $env:PATH
$sdk = 'C:\Program Files (x86)\Windows Mobile 6 SDK\PocketPC'
$cl = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\cl.exe'
$build = Join-Path $PSScriptRoot '..\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null

$flags = @('/nologo', '/TC', '/O1', '/GS-', '/DUNICODE', '/D_UNICODE', '/DUNDER_CE', '/D_WIN32_WCE=0x502', '/DARM', '/D_ARM_', "/I$sdk\Include\Armv4i")
& $cl @flags "/Fo$build\install_rndis.obj" "/Fe$build\install_rndis.exe" (Join-Path $PSScriptRoot 'install_rndis.c') /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if ($LASTEXITCODE -ne 0) { throw 'install_rndis build failed' }

& $cl @flags /DRNDIS_REMOVE "/Fo$build\uninstall_rndis.obj" "/Fe$build\uninstall_rndis.exe" (Join-Path $PSScriptRoot 'install_rndis.c') /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWSCE,5.02 "/LIBPATH:$sdk\Lib\Armv4i" coredll.lib
if ($LASTEXITCODE -ne 0) { throw 'uninstall_rndis build failed' }

if ($BuildOnly) {
    Write-Output "BUILT $(Join-Path $build 'install_rndis.exe') and uninstall_rndis.exe"
    exit 0
}

if ([Environment]::Is64BitProcess) { throw 'Run using SysWOW64 Windows PowerShell' }
$source = Get-Content 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match = [regex]::Match($source, "(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if (!$match.Success) { throw 'RAPI helper missing' }
Add-Type -TypeDefinition $match.Groups[1].Value

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $out = Join-Path $PSScriptRoot '..\evidence\2026-11-25-rndis-deploy'
} else {
    $out = [IO.Path]::GetFullPath($OutDir)
}
New-Item -ItemType Directory -Path $out -Force | Out-Null

try {
    [PositronDeviceRapi]::Connect()
    if ($Remove) {
        [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'uninstall_rndis.exe'), '\Temp\ME3630-rndis-uninstall.exe')
        $remotePid = [PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-rndis-uninstall.exe', '', $null)
        $remoteResult = '\Temp\ME3630-rndis-uninstall.txt'
        $localResult = Join-Path $out 'uninstall.txt'
    } else {
        $rndismp = Join-Path $build 'rndismp.dll'
        $usb8023 = Join-Path $build 'usb8023.dll'
        if (!(Test-Path $rndismp) -or !(Test-Path $usb8023)) {
            throw 'Run scripts\build_rndis_ce5.ps1 first to generate build\rndismp.dll and build\usb8023.dll'
        }
        [PositronDeviceRapi]::CopyFileToDevice($rndismp, '\Windows\rndismp.dll')
        [PositronDeviceRapi]::CopyFileToDevice($usb8023, '\Windows\usb8023.dll')
        [PositronDeviceRapi]::CopyFileToDevice((Join-Path $build 'install_rndis.exe'), '\Temp\ME3630-rndis-install.exe')
        $remotePid = [PositronDeviceRapi]::LaunchProcess('\Temp\ME3630-rndis-install.exe', '', $null)
        $remoteResult = '\Temp\ME3630-rndis-install.txt'
        $localResult = Join-Path $out 'install.txt'
    }
    Write-Output "RNDIS_DEPLOY_PID=$remotePid"
    $finished = $false
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 250
        if ([PositronDeviceRapi]::TryCopyFileFromDevice($remoteResult, $localResult)) {
            $text = Get-Content $localResult -Encoding Unicode -Raw
            if ($text -match 'COMPLETE') {
                Write-Output $text
                $finished = $true
                break
            }
        }
    }
    if (!$finished) { throw 'RNDIS installer/uninstaller did not complete within 10s' }
} finally {
    [PositronDeviceRapi]::Disconnect()
}
