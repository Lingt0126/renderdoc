param(
  [string]$RenderDocDll = '',
  [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $RenderDocDll) { $RenderDocDll = Join-Path $repo 'x64/Development/renderdoc.dll' }
if (-not $OutputDirectory) {
  $OutputDirectory = Join-Path $repo ('../work/dxgi-factory-' + [guid]::NewGuid().ToString('N'))
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
  Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'shared/dxgi1_3.h') } |
  Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $sdkVersion) { throw 'Windows SDK with DXGI headers is required' }
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
  $env:INCLUDE = "$compiler/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um"
  $env:LIB = "$compiler/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
  $exe = Join-Path $out 'dxgi_factory_option_contract.exe'
  $obj = Join-Path $out 'dxgi_factory_option_contract.obj'
  & "$compiler/bin/Hostx64/x64/cl.exe" /nologo /EHsc /W4 "/Fe:$exe" "/Fo:$obj" (Join-Path $PSScriptRoot 'dxgi_factory_option_contract.cpp') /link user32.lib
  if ($LASTEXITCODE -ne 0) { throw 'Contract probe compilation failed' }
  foreach ($mode in @('default', 'modern')) {
    $lines = @(& $exe $dll $mode)
    $code = $LASTEXITCODE
    $lines | Set-Content -LiteralPath (Join-Path $out "$mode.txt") -Encoding UTF8
    $lines | Write-Output
    if ($code -ne 0 -or $lines -notcontains 'DXGI_CONTRACT_COMPLETE failures=0' -or
        @($lines -match '^legacy=[01] CreateDXGIFactory[12]? .* PASS$').Count -ne 3) {
      throw "$mode failed; results in $out"
    }
  }
  [ordered]@{
    dll = $dll
    sha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    result = 'PASS'
    cases = 6
  } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'result.json') -Encoding UTF8
  Write-Output "PASS: default and scoped DXGI factory contracts; evidence: $out"
}
finally {
  $env:INCLUDE = $oldInclude
  $env:LIB = $oldLib
}
