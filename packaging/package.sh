#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
version=$(cat VERSION)
owner=${NIGHTFALL_REPOSITORY:-Miolonixc/nightfall-survivors}
mkdir -p dist/nightfall
cp build/nightfall.aarch64 dist/nightfall/
cp VERSION packaging/update.sh packaging/update-url.txt dist/nightfall/
cp 'packaging/Nightfall Survivors.sh' dist/
cp README.md dist/nightfall/README.md
mkdir -p dist/nightfall/licenses
cp licenses/* dist/nightfall/licenses/
chmod +x dist/nightfall/nightfall.aarch64 'dist/Nightfall Survivors.sh'
cp build/nightfall.aarch64 "dist/nightfall-$version-aarch64"
checksum=$(shasum -a 256 "dist/nightfall-$version-aarch64" | awk '{print $1}')
printf '%s %s %s\n' "$version" "$checksum" "https://github.com/$owner/releases/download/v$version/nightfall-$version-aarch64" > dist/latest-aarch64.txt
(cd dist && zip -qr "nightfall-$version-muos.zip" 'Nightfall Survivors.sh' nightfall)
printf 'Package: %s/dist/nightfall-%s-muos.zip\n' "$root" "$version"
