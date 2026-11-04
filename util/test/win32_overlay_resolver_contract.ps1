param(
  [string]$RenderDocDll = '',
  [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $RenderDocDll) { $RenderDocDll = Join-Path $repo 'x64/Development/renderdoc.dll' }
if (-not $OutputDirectory) {
  $OutputDirectory = Join-Path $repo ('../work/overlay-resolver-' + [guid]::NewGuid().ToString('N'))
}
$dll = (Resolve-Path -LiteralPath $RenderDocDll).Path
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$out = (Resolve-Path -LiteralPath $OutputDirectory).Path
$vswhere = 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools are required' }
$version = (Get-Content -LiteralPath (Join-Path $vs 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$compiler = Join-Path $vs "VC/Tools/MSVC/$version"
$sdk = 'C:/Program Files (x86)/Windows Kits/10'
$sdkVersion = Get-ChildItem -LiteralPath "$sdk/Include" -Directory |
  Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um/windows.h') } |
  Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $sdkVersion) { throw 'Windows SDK is required' }
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
  $env:INCLUDE = "$compiler/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um"
  $env:LIB = "$compiler/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
  $cl = "$compiler/bin/Hostx64/x64/cl.exe"
  $fixture = Join-Path $out 'OrdinaryHookClient.dll'
  $exe = Join-Path $out 'win32_overlay_resolver_contract.exe'
  & $cl /nologo /EHsc /W4 /MT /LD "/Fe:$fixture" "/Fo:$out/fixture.obj" (Join-Path $PSScriptRoot 'win32_overlay_resolver_fixture.cpp') /link "/IMPLIB:$out/fixture.lib"
  if ($LASTEXITCODE -ne 0) { throw 'Fixture compilation failed' }
  & $cl /nologo /EHsc /W4 /MT "/Fe:$exe" "/Fo:$out/contract.obj" (Join-Path $PSScriptRoot 'win32_overlay_resolver_contract.cpp')
  if ($LASTEXITCODE -ne 0) { throw 'Contract compilation failed' }
  foreach ($name in @('EOSOVH-Win64-Shipping.dll', 'EOSOVH-Win32-Shipping.dll')) {
    $overlay = Join-Path $out $name
    Copy-Item -LiteralPath $fixture -Destination $overlay -Force
    $lines = @(& $exe $dll $overlay $fixture)
    $code = $LASTEXITCODE
    $lines | Set-Content -LiteralPath (Join-Path $out "$name.txt")
    $lines | Write-Output
    if ($code -ne 0 -or -not ($lines -match 'application_hooked=1 overlay_native=1 ordinary_hooked=1 missing_export=1 PASS')) {
      throw "Resolver coexistence contract failed for $name; evidence: $out"
    }
  }
  [ordered]@{ dll=$dll; sha256=(Get-FileHash $dll -Algorithm SHA256).Hash; result='PASS' } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'result.json')
  Write-Output "OVERLAY_RESOLVER_COMPLETE failures=0 evidence=$out"
}
finally {
  $env:INCLUDE = $oldInclude
  $env:LIB = $oldLib
}
