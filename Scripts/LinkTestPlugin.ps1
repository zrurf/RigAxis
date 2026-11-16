param(
    [switch]$PaperZD
)

$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..")

function Link-Plugin([string]$Name, [string]$Source, [string]$PluginRoot)
{
    New-Item -ItemType Directory -Force $PluginRoot | Out-Null
    Copy-Item (Join-Path $Source "$Name.uplugin") (Join-Path $PluginRoot "$Name.uplugin") -Force

    foreach ($entry in @("Source", "Content", "Resources", "Config"))
    {
        $target = Join-Path $Source $entry
        if (-not (Test-Path $target))
        {
            continue
        }

        $link = Join-Path $PluginRoot $entry
        if (Test-Path $link)
        {
            $item = Get-Item $link -Force
            if (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint))
            {
                throw "Refusing to replace non-link path: $link"
            }
            cmd /c rmdir $link | Out-Null
        }

        New-Item -ItemType Junction -Path $link -Target $target | Out-Null
    }

    Write-Host "Prepared test plugin shim: $PluginRoot"
}

Link-Plugin "RigAxis" $repo (Join-Path $repo "Tests\RigAxisTest\Plugins\RigAxis")

if ($PaperZD)
{
    $adapter = Join-Path $repo "RigAxisPaperZD"
    if (-not (Test-Path $adapter))
    {
        throw "Missing $adapter"
    }
    Link-Plugin "RigAxisPaperZD" $adapter (Join-Path $repo "Tests\RigAxisTest\Plugins\RigAxisPaperZD")
}
