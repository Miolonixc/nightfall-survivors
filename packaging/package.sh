#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
version=$(cat VERSION)
owner=${NIGHTFALL_REPOSITORY:-Miolonixc/nightfall-survivors}
rm -rf "$root/dist/nightfall"
mkdir -p dist/nightfall
cp build/nightfall.aarch64 dist/nightfall/
cp VERSION packaging/update.sh dist/nightfall/
printf '%s\n' "https://github.com/$owner/releases/latest/download/latest-aarch64.txt" > dist/nightfall/update-url.txt
cp 'packaging/Nightfall Survivors.sh' dist/
cp README.md DESIGN.md CHANGELOG.md screenshot.png dist/nightfall/
cp -R docs dist/nightfall/
cp docs/INSTALL-RU.md dist/INSTALL-RU.md
mkdir -p dist/nightfall/licenses
cp licenses/* dist/nightfall/licenses/
chmod +x dist/nightfall/nightfall.aarch64 'dist/Nightfall Survivors.sh'
cp build/nightfall.aarch64 "dist/nightfall-$version-aarch64"
checksum=$(shasum -a 256 "dist/nightfall-$version-aarch64" | awk '{print $1}')
printf '%s %s %s\n' "$version" "$checksum" "https://github.com/$owner/releases/download/v$version/nightfall-$version-aarch64" > dist/latest-aarch64.txt
rm -f "dist/nightfall-$version-muos.zip" "dist/nightfall-$version-docs.zip"
(cd dist && zip -qr "nightfall-$version-muos.zip" 'Nightfall Survivors.sh' nightfall INSTALL-RU.md)
zip -qr "dist/nightfall-$version-docs.zip" README.md DESIGN.md CHANGELOG.md screenshot.png docs
printf 'Package: %s/dist/nightfall-%s-muos.zip\n' "$root" "$version"
