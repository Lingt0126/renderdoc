param(
  [string]$RenderDocDll = '',
  [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $RenderDocDll) { $RenderDocDll = Join-Path $repo 'x64/Development/renderdoc.dll' }
if (-not $OutputDirectory) {
  $OutputDirectory = Join-Path $repo ('../work/d3d11-optout-' + [guid]::NewGuid().ToString('N'))
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
  Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um/d3d11.h') } |
  Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $sdkVersion) { throw 'Windows SDK with D3D11 headers is required' }
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
  $env:INCLUDE = "$compiler/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um"
  $env:LIB = "$compiler/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
  $exe = Join-Path $out 'd3d11_optout_contract.exe'
  $obj = Join-Path $out 'd3d11_optout_contract.obj'
  & "$compiler/bin/Hostx64/x64/cl.exe" /nologo /EHsc /W4 "/Fe:$exe" "/Fo:$obj" (Join-Path $PSScriptRoot 'd3d11_optout_contract.cpp') /link user32.lib
  if ($LASTEXITCODE -ne 0) { throw 'Contract probe compilation failed' }
  foreach ($mode in @('native', 'renderdoc', 'capture-optout')) {
    if ($mode -eq 'native') { $lines = @(& $exe) }
    elseif ($mode -eq 'capture-optout') { $lines = @(& $exe $dll '--capture-optout') }
    else { $lines = @(& $exe $dll) }
    $code = $LASTEXITCODE
    $lines | Set-Content -LiteralPath (Join-Path $out "$mode.txt") -Encoding UTF8
    $lines | Write-Output
    if ($code -ne 0 -or $lines -notcontains 'CONTRACT_COMPLETE failures=0') {
      throw "$mode failed; results in $out"
    }
    if ($mode -eq 'renderdoc' -and -not ($lines -match 'optout=0 outputs=3 .*wrapped=1 PASS')) {
      throw 'RenderDoc did not wrap the ordinary device; test would be a false positive'
    }
    if ($mode -eq 'capture-optout' -and -not ($lines -match 'optout=1 outputs=3 .*wrapped=1 PASS')) {
      throw 'Explicit capture override did not wrap the opted-out device'
    }
  }
  [ordered]@{
    dll = $dll
    sha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    result = 'PASS'
  } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'result.json') -Encoding UTF8
  Write-Output "PASS: native and RenderDoc contracts; evidence: $out"
}
finally {
  $env:INCLUDE = $oldInclude
  $env:LIB = $oldLib
}
