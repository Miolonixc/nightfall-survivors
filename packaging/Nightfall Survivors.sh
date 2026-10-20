#!/bin/bash
# PortMaster environment and native SDL controller mapping.
controlfolder=''
for candidate in /opt/system/Tools/PortMaster /opt/tools/PortMaster "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster" /mnt/mmc/MUOS/PortMaster /mnt/sdcard/MUOS/PortMaster /roms/ports/PortMaster; do
    if [ -f "$candidate/control.txt" ]; then controlfolder=$candidate; break; fi
done
if [ -n "$controlfolder" ]; then
    source "$controlfolder/control.txt"
    [ ! -f "$controlfolder/mod_${CFW_NAME}.txt" ] || source "$controlfolder/mod_${CFW_NAME}.txt"
    get_controls
    export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi
scriptdir=$(cd -- "$(dirname -- "$0")" && pwd)
# Prefer sibling data; also support muOS ROMS/PORTS with data in /ports.
GAMEDIR="$scriptdir/nightfall"
if [ ! -d "$GAMEDIR" ] && [ -n "${directory:-}" ]; then GAMEDIR="/$directory/ports/nightfall"; fi
if [ ! -d "$GAMEDIR" ]; then
    for base in /mnt/mmc/ports /mnt/sdcard/ports /roms/ports; do
        [ ! -d "$base/nightfall" ] || { GAMEDIR="$base/nightfall"; break; }
    done
fi
cd "$GAMEDIR" || exit 1
# mkdir is released only after the launched process has stopped.
if [ -d .launch-lock ]; then
    owner=$(cat .launch-lock/pid 2>/dev/null || true)
    case "$owner" in ''|*[!0-9]*) rmdir .launch-lock 2>/dev/null || true;;
    *) if ! kill -0 "$owner" 2>/dev/null; then rm -f .launch-lock/pid; rmdir .launch-lock 2>/dev/null || true; fi;; esac
fi
mkdir .launch-lock 2>/dev/null || exit 0
printf '%s\n' "$$" > .launch-lock/pid
trap 'rm -f "$GAMEDIR/.launch-lock/pid"; rmdir "$GAMEDIR/.launch-lock" 2>/dev/null || true' EXIT
exec > >(tee -a log.txt) 2>&1
export NIGHTFALL_DATA="$GAMEDIR/save"
mkdir -p "$NIGHTFALL_DATA"
feed=$(cat update-url.txt 2>/dev/null || true)
# A previous launch or installation was interrupted before the health marker.
rollback() {
    [ -f nightfall.previous ] || return 1
    cp -p nightfall.previous nightfall.restore && mv -f nightfall.restore nightfall.aarch64 || return 1
    [ ! -f VERSION.previous ] || cp VERSION.previous VERSION
    [ ! -f update.pending ] || mv -f update.pending failed-version
    printf '%s\n' 'Restored previous working version.'
}
if [ -f update.pending ] && [ ! -f launch.ready ]; then rollback || true; fi
if [ -f update.pending ] && [ -f launch.ready ]; then rm -f update.pending; fi
rm -f launch.ready
[ -z "$feed" ] || sh ./update.sh "$GAMEDIR" "$feed" || true
if [ -n "$controlfolder" ]; then pm_platform_helper "$GAMEDIR/nightfall.aarch64"; fi
./nightfall.aarch64 --fullscreen --ready-file "$GAMEDIR/launch.ready" "$@"
status=$?
if [ -f update.pending ]; then
    if [ -f launch.ready ]; then
        rm -f update.pending failed-version
    else
        rollback || true
        # Start the restored game immediately, including after an early crash.
        ./nightfall.aarch64 --fullscreen --ready-file "$GAMEDIR/launch.ready" "$@"
        status=$?
    fi
fi
if [ -n "$controlfolder" ]; then pm_finish; fi
exit "$status"
