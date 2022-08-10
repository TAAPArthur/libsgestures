#!/bin/sh -e

SGESTURES_DATA_DIR=${XDG_DATA_DIR:-$HOME/.local/.share}/sgestures
SGESTURES_BIN="$SGESTURES_DATA_DIR/${SGESTURES_BIN_NAME:-sgestures}"
SGESTURES_HOME=${SGESTURES_HOME:-${XDG_CONFIG_HOME:-$HOME/.config}/sgestures}

recompile() {
    if [ ! -d "$SGESTURES_HOME" ]; then
        mkdir -p "$SGESTURES_HOME"
        cp /usr/share/sgestures/sample-gesture-reader.c "$SGESTURES_HOME/config.c"
    fi
    mkdir -p "$SGESTURES_DATA_DIR"
    # shellcheck disable=SC2086
    ${CC:-cc} "$SGESTURES_HOME"/*.c -o "$SGESTURES_BIN" $CFLAGS $LDFLAGS -lsgestures -lm "$@"
}

if [ "$1" = "-r" ] || [ "$1" = "--recompile" ]; then
    shift
    recompile "$@"
    exit
fi
[ -d "$SGESTURES_HOME" ] || exec /usr/libexec/sgestures

[ -x "$SGESTURES_BIN" ] || recompile
exec "$SGESTURES_BIN"
