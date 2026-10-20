#!/bin/sh
# HTTPS release manifest: VERSION SHA256 HTTPS_BINARY_URL (one line).
# Never source network data or unpack remote archives. One atomic binary replacement.
set -eu
gamedir=${1:?game directory required}
feed=${2:?manifest URL required}
cd "$gamedir"
log() { printf '%s\n' "Updater: $*"; }
[ -f updates.disabled ] && exit 0
case "$feed" in https://*) ;; *) log 'HTTPS feed required; keeping installed game'; exit 0;; esac
for tool in curl sha256sum od awk; do
    command -v "$tool" >/dev/null 2>&1 || { log "$tool unavailable; keeping installed game"; exit 0; }
done
# mkdir is the lock, so parallel launches cannot replace the same executable.
if [ -d .update-lock ]; then
    owner=$(cat .update-lock/pid 2>/dev/null || true)
    case "$owner" in ''|*[!0-9]*) rmdir .update-lock 2>/dev/null || true;;
    *) if ! kill -0 "$owner" 2>/dev/null; then rm -f .update-lock/pid; rmdir .update-lock 2>/dev/null || true; fi;; esac
fi
if ! mkdir .update-lock 2>/dev/null; then log 'another update is active; skipping'; exit 0; fi
printf '%s\n' "$$" > .update-lock/pid
tmp=''
cleanup() { [ -z "$tmp" ] || rm -rf "$tmp"; rm -f .update-lock/pid; rmdir .update-lock 2>/dev/null || true; }
trap cleanup EXIT HUP INT TERM
tmp=$(mktemp -d "$gamedir/.update.XXXXXX")
fetch() { curl --fail --silent --show-error --location --proto '=https' --proto-redir '=https' --connect-timeout 3 --max-time "$3" --max-filesize "$4" "$1" -o "$2"; }
log 'checking for updates...'
if ! fetch "$feed" "$tmp/manifest" 8 4096; then log 'offline or no release available; starting installed game'; exit 0; fi
remote='' digest='' url='' extra=''
IFS=' ' read -r remote digest url extra < "$tmp/manifest" || { log 'invalid manifest'; exit 0; }
if [ "$(wc -l < "$tmp/manifest" | tr -d ' ')" != 1 ] || [ -n "$extra" ]; then log 'invalid manifest'; exit 0; fi
if ! printf '%s\n' "$remote" | LC_ALL=C grep -Eq '^[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}$'; then log 'invalid version'; exit 0; fi
if ! printf '%s\n' "$digest" | LC_ALL=C grep -Eq '^[a-f0-9]{64}$'; then log 'invalid checksum'; exit 0; fi
case "$url" in https://*) ;; *) log 'HTTPS download required'; exit 0;; esac
current=$(cat VERSION 2>/dev/null || printf '0.0.0')
if ! awk -v old="$current" -v new="$remote" 'BEGIN {split(old,a,".");split(new,b,".");for(i=1;i<=3;i++){if(b[i]+0>a[i]+0)exit 0;if(b[i]+0<a[i]+0)exit 1;}exit 1;}'; then log "installed version $current is current"; exit 0; fi
if [ -f failed-version ] && [ "$(cat failed-version)" = "$remote" ]; then log "version $remote previously failed; keeping $current"; exit 0; fi
log "downloading $remote..."
if ! fetch "$url" "$tmp/nightfall.aarch64" 60 8388608; then log 'download failed; keeping installed game'; exit 0; fi
actual=$(sha256sum "$tmp/nightfall.aarch64" | awk '{print $1}')
if [ "$actual" != "$digest" ]; then log 'checksum mismatch; keeping installed game'; exit 0; fi
magic=$(od -An -tx1 -N5 "$tmp/nightfall.aarch64" | tr -d ' \n')
machine=$(od -An -tx1 -j18 -N2 "$tmp/nightfall.aarch64" | tr -d ' \n')
if [ "$magic" != 7f454c4602 ] || [ "$machine" != b700 ]; then log 'not a Linux ARM64 executable; keeping installed game'; exit 0; fi
chmod +x "$tmp/nightfall.aarch64"
if [ -f nightfall.aarch64 ]; then cp -p nightfall.aarch64 "$tmp/previous"; mv -f "$tmp/previous" nightfall.previous; fi
printf '%s\n' "$current" > VERSION.previous
printf '%s\n' "$remote" > "$tmp/VERSION"
# Pending marker precedes replacement, so an interrupted installation can roll back.
printf '%s\n' "$remote" > update.pending
mv -f "$tmp/nightfall.aarch64" nightfall.aarch64
mv -f "$tmp/VERSION" VERSION
log "installed $remote; previous executable retained until successful startup"
