$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$rust = Join-Path $root "Rust"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found"
}
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Visual Studio with C++ tools not found"
}
$devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"

$command = "call `"$devCmd`" -arch=amd64 -host_arch=amd64 >nul && cd /d `"$rust`" && cargo test -p rigaxis-ffi --all-targets && cargo clippy -p rigaxis-ffi --all-targets --no-deps -- -D warnings"
& cmd.exe /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Rust tests or clippy failed"
}

Write-Host "Rust tests and clippy passed."
