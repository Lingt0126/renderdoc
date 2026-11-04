param(
  [string]$RenderDocDll = '',
  [string]$RenderDocLib = '',
  [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $RenderDocDll) { $RenderDocDll = Join-Path $repo 'x64/Development/renderdoc.dll' }
if (-not $OutputDirectory) {
  $OutputDirectory = Join-Path $repo ('../work/win32-launch-' + [guid]::NewGuid().ToString('N'))
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
$oldPath = $env:PATH
try {
  $env:INCLUDE = "$compiler/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um"
  $env:LIB = "$compiler/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
  if (-not $RenderDocLib) { $RenderDocLib = Join-Path $repo 'x64/Development/renderdoc.lib' }
  $env:PATH = (Split-Path -Parent $dll) + ';' + $oldPath
  $exe = Join-Path $out 'win32_launch_contract.exe'
  $target = Join-Path $out 'win32_launch_contract_target.exe'
  & "$compiler/bin/Hostx64/x64/cl.exe" /nologo /EHsc /W4 /MT /DWIN32 "/I$repo" "/Fe:$exe" "/Fo:$out/controller.obj" (Join-Path $PSScriptRoot 'win32_launch_contract.cpp') /link $RenderDocLib
  if ($LASTEXITCODE -ne 0) { throw 'Controller compilation failed' }
  & "$compiler/bin/Hostx64/x64/cl.exe" /nologo /EHsc /W4 /MT "/Fe:$target" "/Fo:$out/target.obj" (Join-Path $PSScriptRoot 'win32_launch_contract_target.cpp') /link /SUBSYSTEM:WINDOWS
  if ($LASTEXITCODE -ne 0) { throw 'Target compilation failed' }
  foreach ($mode in @('ordinary', 'bootstrap', 'invalid-pid')) {
    $lines = @(& $exe $target $mode (Join-Path $out "$mode.log"))
    $code = $LASTEXITCODE
    $lines | Set-Content (Join-Path $out "$mode.txt")
    $lines | Write-Output
    if ($code -ne 0 -or -not ($lines -match "^mode=$mode .* PASS$")) {
      throw "$mode contract failed; evidence: $out"
    }
  }
  $log = Get-Content -LiteralPath (Join-Path $out 'bootstrap.log.saved') -Raw
  if ($log -notmatch 'DLL load thread:' -or $log -notmatch 'Replacement capture result:.*success=0, controlIdent=0') {
    throw 'Missing loader or replacement failure diagnostics'
  }
  [ordered]@{dll=$dll; sha256=(Get-FileHash $dll -Algorithm SHA256).Hash; result='PASS'} |
    ConvertTo-Json | Set-Content (Join-Path $out 'result.json')
  Write-Output "LAUNCH_CONTRACT_COMPLETE failures=0 evidence=$out"
}
finally {
  $env:INCLUDE = $oldInclude
  $env:LIB = $oldLib
  $env:PATH = $oldPath
}
