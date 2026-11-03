param(
    [ValidateSet("win64")] [string]$Target = "win64",
    [ValidateSet("Release", "Debug")] [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
$rustDir = Join-Path $repo "Rust"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere))
{
    throw "vswhere.exe not found"
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath)
{
    throw "Visual Studio with C++ tools not found"
}

$devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
$triple = "x86_64-pc-windows-msvc"
$profile = if ($Configuration -eq "Release") { "release" } else { "debug" }
$cargoArgs = if ($Configuration -eq "Release") {
    "build --release -p rigaxis-ffi --target $triple"
} else {
    "build -p rigaxis-ffi --target $triple"
}

$command = "call `"$devCmd`" -arch=amd64 -host_arch=amd64 >nul && cd /d `"$rustDir`" && cargo $cargoArgs"
& cmd.exe /c $command
if ($LASTEXITCODE -ne 0)
{
    throw "cargo build failed"
}

$src = Join-Path $rustDir "target/$triple/$profile/rigaxis_ffi.lib"
$dstDir = Join-Path $repo "Source/ThirdParty/RigAxis/lib/Win64/$Configuration"
New-Item -ItemType Directory -Force $dstDir | Out-Null
Copy-Item $src $dstDir -Force

Write-Host "Built $src"
Write-Host "Copied to $dstDir"
