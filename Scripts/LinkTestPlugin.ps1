$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
$pluginRoot = Join-Path $repo "Tests\RigAxisTest\Plugins\RigAxis"

New-Item -ItemType Directory -Force $pluginRoot | Out-Null
Copy-Item (Join-Path $repo "RigAxis.uplugin") (Join-Path $pluginRoot "RigAxis.uplugin") -Force

foreach ($name in @("Source", "Content", "Resources", "Config"))
{
    $target = Join-Path $repo $name
    if (-not (Test-Path $target))
    {
        continue
    }

    $link = Join-Path $pluginRoot $name
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

Write-Host "Prepared test plugin shim: $pluginRoot"
