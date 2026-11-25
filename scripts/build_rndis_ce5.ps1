param(
    [string]$CeRoot = 'C:\WINCE500',
    [string]$PeOut = ''
)
$ErrorActionPreference = 'Stop'

$env:PATH = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;' + $env:PATH
$lib = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\lib.exe'
$link = 'C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\ce\bin\x86_arm\link.exe'

$oak = Join-Path $CeRoot 'PUBLIC\COMMON\OAK\LIB\ARMV4I\RETAIL'
$sdk5 = Join-Path $CeRoot 'PUBLIC\COMMON\SDK\LIB\ARMV4I\RETAIL'
$build = Join-Path $PSScriptRoot '..\build'
$tmp = Join-Path $build 'ce5_rndis_tmp'

foreach ($req in @("$oak\rndismp.lib", "$oak\rndismp.def", "$oak\usb8023.lib", "$oak\usb8023.def", "$oak\usbclient.lib", "$sdk5\ntcompat.lib", "$sdk5\corelibc.lib")) {
    if (!(Test-Path $req)) { throw "Missing CE5 PB input: $req" }
}

New-Item -ItemType Directory -Path $build -Force | Out-Null
New-Item -ItemType Directory -Path $tmp -Force | Out-Null

foreach ($d in @('ndis', 'cxport', 'usbd', 'ceddk', 'coredll')) {
    & $lib /NOLOGO /MACHINE:THUMB "/DEF:$oak\$d.def" "/OUT:$tmp\$d.lib"
    if ($LASTEXITCODE -ne 0) { throw "Import library generation failed for $d.def" }
}

$rndismpDll = Join-Path $build 'rndismp.dll'
$usb8023Dll = Join-Path $build 'usb8023.dll'

& $link /NOLOGO /DLL /MACHINE:THUMB /SUBSYSTEM:WINDOWSCE,5.00 /NODEFAULTLIB /ENTRY:DllEntry `
    "/DEF:$oak\rndismp.def" "/OUT:$rndismpDll" "/IMPLIB:$tmp\rndismp_linked.lib" `
    "$oak\rndismp.lib" "$tmp\ceddk.lib" "$sdk5\ntcompat.lib" "$tmp\ndis.lib" "$tmp\cxport.lib" "$sdk5\corelibc.lib" "$tmp\coredll.lib"
if ($LASTEXITCODE -ne 0) { throw 'Link failed for rndismp.dll' }

& $link /NOLOGO /DLL /MACHINE:THUMB /SUBSYSTEM:WINDOWSCE,5.00 /NODEFAULTLIB /ENTRY:DllEntry `
    "/DEF:$oak\usb8023.def" "/OUT:$usb8023Dll" "/IMPLIB:$tmp\usb8023_linked.lib" `
    "$oak\usb8023.lib" "$tmp\ndis.lib" "$tmp\cxport.lib" "$sdk5\ntcompat.lib" "$tmp\usbd.lib" "$tmp\rndismp_linked.lib" "$oak\usbclient.lib" "$sdk5\corelibc.lib" "$tmp\coredll.lib"
if ($LASTEXITCODE -ne 0) { throw 'Link failed for usb8023.dll' }

$peJson = python (Join-Path $PSScriptRoot 'inspect_pe.py') $rndismpDll $usb8023Dll
if ($LASTEXITCODE -ne 0) { throw 'inspect_pe.py failed' }

if (-not [string]::IsNullOrWhiteSpace($PeOut)) {
    $dir = Split-Path -Parent $PeOut
    if ($dir) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $peJson | Set-Content -Path $PeOut -Encoding UTF8
}
$peJson
