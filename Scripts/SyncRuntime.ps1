param(
    [Parameter(Mandatory = $true)][string]$Repo,
    [Parameter(Mandatory = $true)][string]$Ref,
    [string]$Date = ""
)

$ErrorActionPreference = "Stop"

$utf8 = New-Object System.Text.UTF8Encoding($false)
function Copy-Text([string]$Source, [string]$Destination) {
    $text = [System.IO.File]::ReadAllText($Source)
    $text = $text.Replace("`r`n", "`n")
    [System.IO.File]::WriteAllText($Destination, $text, $utf8)
}

$repository = (Resolve-Path $Repo).Path
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$vendor = Join-Path $root "Rust/vendor/rigby2d"
$runtime = Join-Path $repository "runtimes/rust/rigby-runtime"
$runtimeTarget = Join-Path $vendor "runtimes/rust/rigby-runtime"

$head = (& git -C $repository rev-parse "$Ref^{commit}").Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Cannot resolve $Ref in $repository"
}
$checkedOut = (& git -C $repository rev-parse HEAD).Trim()
if ($checkedOut -ne $head) {
    throw "Check out $Ref in $repository before syncing"
}
if (-not (Test-Path $runtime)) {
    throw "Missing $runtime"
}
if (-not $Date) {
    $Date = (& git -C $repository show -s --format=%cs $head).Trim()
}

$workspace = Get-Content (Join-Path $repository "runtimes/rust/Cargo.toml") -Raw
$versionMatch = [regex]::Match($workspace, '(?m)^version\s*=\s*"([^"]+)"')
if (-not $versionMatch.Success) {
    throw "Cannot read the runtime version"
}
$version = $versionMatch.Groups[1].Value

New-Item -ItemType Directory -Force (Join-Path $runtimeTarget "src") | Out-Null
Get-ChildItem (Join-Path $runtimeTarget "src") -Filter *.rs -File | Remove-Item -Force
Copy-Text (Join-Path $runtime "Cargo.toml") (Join-Path $runtimeTarget "Cargo.toml")
Get-ChildItem (Join-Path $runtime "src") -Filter *.rs -File | ForEach-Object {
    Copy-Text $_.FullName (Join-Path (Join-Path $runtimeTarget "src") $_.Name)
}
Copy-Text (Join-Path $repository "LICENSE-MIT") (Join-Path $vendor "LICENSE-MIT")
Copy-Text (Join-Path $repository "LICENSE-APACHE") (Join-Path $vendor "LICENSE-APACHE")

$upstream = (@(
    "# Rigby2D runtime snapshot"
    ""
    "- Repository: https://github.com/jaisurya-dev-art/rigby2d"
    "- Commit: ``$head``"
    "- Upstream release: ``$Ref``"
    "- Date: $Date"
    "- Crate: ``runtimes/rust/rigby-runtime``"
    "- Version: $version"
    "- License: MIT OR Apache-2.0"
    "- Vendored files: ``Cargo.toml``, ``src/*.rs``, ``LICENSE-MIT``, ``LICENSE-APACHE``"
    "- Local modifications: none"
    ""
    "Tests, examples and demo assets are not vendored. See ``THIRD_PARTY_NOTICES.md`` at the repository root."
) -join "`n") + "`n"

[System.IO.File]::WriteAllText((Join-Path $vendor "UPSTREAM.md"), $upstream, $utf8)

$noticesPath = Join-Path $root "THIRD_PARTY_NOTICES.md"
$notices = [System.IO.File]::ReadAllText($noticesPath).Replace("`r`n", "`n")
$notices = [regex]::Replace($notices, '- Commit: `[0-9a-f]+`', "- Commit: ``$head``")
$notices = [regex]::Replace($notices, '- Upstream release: `[^`]+`', "- Upstream release: ``$Ref``")
[System.IO.File]::WriteAllText($noticesPath, $notices, $utf8)

Write-Host "Synced $Ref ($head) into $vendor"
Write-Host "Next: run Scripts/TestRust.ps1, rebuild, and run the UE tests."
