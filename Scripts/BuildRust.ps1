param(
    [ValidateSet("win64", "android", "all")] [string]$Target = "win64",
    [ValidateSet("Release", "Debug")] [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
$rustDir = Join-Path $repo "Rust"
$profile = if ($Configuration -eq "Release") { "release" } else { "debug" }
$cargoArgs = @("build")
if ($Configuration -eq "Release")
{
    $cargoArgs += "--release"
}

function Build-Win64
{
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
    $cargoCommand = $cargoArgs -join " "
    $command = "call `"$devCmd`" -arch=amd64 -host_arch=amd64 >nul && cd /d `"$rustDir`" && cargo $cargoCommand -p rigaxis-ffi --target $triple"
    & cmd.exe /c $command
    if ($LASTEXITCODE -ne 0)
    {
        throw "cargo build failed for $triple"
    }

    $src = Join-Path $rustDir "target/$triple/$profile/rigaxis_ffi.lib"
    $dstDir = Join-Path $repo "Source/ThirdParty/RigAxis/lib/Win64/$Configuration"
    New-Item -ItemType Directory -Force $dstDir | Out-Null
    Copy-Item $src $dstDir -Force

    Write-Host "Built $src"
    Write-Host "Copied to $dstDir"
}

function Build-Android
{
    $ndk = $env:ANDROID_NDK_HOME
    if (-not $ndk)
    {
        $ndk = $env:NDK_HOME
    }
    if (-not $ndk)
    {
        throw "ANDROID_NDK_HOME is not set"
    }

    $toolchain = Join-Path $ndk "toolchains/llvm/prebuilt/windows-x86_64/bin"
    $api = "26"
    $targets = @(
        @{ Triple = "aarch64-linux-android"; Arch = "arm64"; Linker = "aarch64-linux-android$api-clang.cmd" },
        @{ Triple = "x86_64-linux-android"; Arch = "x86_64"; Linker = "x86_64-linux-android$api-clang.cmd" }
    )

    $previousRustFlags = $env:RUSTFLAGS
    $env:RUSTFLAGS = "-g"
    try
    {
        foreach ($item in $targets)
        {
            $linker = Join-Path $toolchain $item.Linker
            if (-not (Test-Path $linker))
            {
                throw "Android linker not found: $linker"
            }

            $envName = "CARGO_TARGET_" + ($item.Triple.ToUpper() -replace "-", "_") + "_LINKER"
            Set-Item -Path "env:$envName" -Value $linker

            Push-Location $rustDir
            try
            {
                & cargo @cargoArgs -p rigaxis-ffi --target $item.Triple
                if ($LASTEXITCODE -ne 0)
                {
                    throw "cargo build failed for $($item.Triple)"
                }
            }
            finally
            {
                Pop-Location
            }

            $src = Join-Path $rustDir "target/$($item.Triple)/$profile/librigaxis_ffi.a"
            $dstDir = Join-Path $repo "Source/ThirdParty/RigAxis/lib/Android/$($item.Arch)"
            New-Item -ItemType Directory -Force $dstDir | Out-Null
            Copy-Item $src (Join-Path $dstDir "rigaxis_ffi.a") -Force

            Write-Host "Built $src"
            Write-Host "Copied to $dstDir"
        }
    }
    finally
    {
        if ($null -eq $previousRustFlags)
        {
            Remove-Item Env:RUSTFLAGS -ErrorAction SilentlyContinue
        }
        else
        {
            $env:RUSTFLAGS = $previousRustFlags
        }
    }
}

switch ($Target)
{
    "win64" { Build-Win64 }
    "android" { Build-Android }
    "all"
    {
        Build-Win64
        Build-Android
    }
}
