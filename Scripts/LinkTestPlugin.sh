#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
plugin_root="$repo/Tests/RigAxisTest/Plugins/RigAxis"

mkdir -p "$plugin_root"
cp "$repo/RigAxis.uplugin" "$plugin_root/RigAxis.uplugin"

for name in Source Content Resources Config; do
    target="$repo/$name"
    [ -e "$target" ] || continue

    link="$plugin_root/$name"
    if [ -L "$link" ]; then
        rm "$link"
    elif [ -e "$link" ]; then
        echo "Refusing to replace non-link path: $link" >&2
        exit 1
    fi

    ln -s "$target" "$link"
done

echo "Prepared test plugin shim: $plugin_root"
