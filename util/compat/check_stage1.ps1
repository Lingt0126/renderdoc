param(
  [Parameter(Mandatory=$true)][string]$BuildRoot,
  [string]$ReplayCapture = '',
  [ValidateSet('x64','Win32')][string[]]$Platforms = @('x64','Win32')
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$source = Join-Path $BuildRoot 'source'
$evidence = Join-Path $BuildRoot 'checks'
$null = New-Item -ItemType Directory -Path $evidence -Force
foreach ($arch in $Platforms) {
  $runtime = Join-Path $source "$arch/Development"
  $info = [Diagnostics.ProcessStartInfo]::new()
  $info.FileName = Join-Path $runtime 'rendertestcmd.exe'
  $info.Arguments = 'test unit'
  $info.WorkingDirectory = $runtime
  $info.UseShellExecute = $false
  $info.CreateNoWindow = $true
  $info.RedirectStandardOutput = $true
  $info.RedirectStandardError = $true
  $info.EnvironmentVariables['HOME'] = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
  $proc = [Diagnostics.Process]::Start($info)
  $stdout = $proc.StandardOutput.ReadToEndAsync()
  $stderr = $proc.StandardError.ReadToEndAsync()
  $proc.WaitForExit()
  $stdout.Result | Set-Content (Join-Path $evidence "unit-$arch.txt")
  $stderr.Result | Set-Content (Join-Path $evidence "unit-$arch.stderr.txt")
  if ($proc.ExitCode -ne 0) { throw "Core unit tests failed for $arch" }
  $guiLog = Join-Path $evidence "gui-$arch.txt"
  $gui = Start-Process -FilePath (Join-Path $runtime 'qrendertest.exe') -ArgumentList @('--unittest',"log=$guiLog") -WindowStyle Hidden -Wait -PassThru
  if ($gui.ExitCode -ne 0) { throw "GUI unit tests failed for $arch" }
  Write-Output "PASS core and GUI unit tests: $arch"
}
$runtime64 = Join-Path $source 'x64/Development'
foreach ($contract in @('d3d11_optout_contract','dxgi_factory_option_contract','win32_overlay_resolver_contract')) {
  & (Join-Path $repo "util/test/$contract.ps1") -RenderDocDll (Join-Path $runtime64 'rendertest.dll') -OutputDirectory (Join-Path $evidence $contract) *> (Join-Path $evidence "$contract.log")
  if (-not $?) { throw "$contract failed" }
  Write-Output "PASS $contract"
}
& (Join-Path $repo 'util/test/win32_launch_contract.ps1') -RenderDocDll (Join-Path $runtime64 'rendertest.dll') -RenderDocLib (Join-Path $runtime64 'rendertest.lib') -OutputDirectory (Join-Path $evidence 'win32_launch_contract') *> (Join-Path $evidence 'win32_launch_contract.log')
if (-not $?) { throw 'win32_launch_contract failed' }
Write-Output 'PASS win32_launch_contract'
if ($ReplayCapture) {
  $info = [Diagnostics.ProcessStartInfo]::new()
  $info.FileName = Join-Path $runtime64 'qrendertest.exe'
  $info.Arguments = '--python "' + (Join-Path $PSScriptRoot 'replay_smoke.py') + '"'
  $info.WorkingDirectory = $runtime64
  $info.UseShellExecute = $false
  $info.CreateNoWindow = $true
  $info.EnvironmentVariables['COMPAT_SMOKE_RDC'] = (Resolve-Path -LiteralPath $ReplayCapture).Path
  $info.EnvironmentVariables['COMPAT_SMOKE_PNG'] = Join-Path $evidence 'replay.png'
  $info.EnvironmentVariables['COMPAT_SMOKE_RESULT'] = Join-Path $evidence 'replay.json'
  $proc = [Diagnostics.Process]::Start($info)
  $proc.WaitForExit()
  $result = Get-Content (Join-Path $evidence 'replay.json') -Raw | ConvertFrom-Json
  if ($result.result -ne 'PASS') { throw "Replay check failed: $($result.error)" }
  Write-Output "PASS independent replay: $($result.driver), $($result.actions) actions, $($result.textures) textures"
}
