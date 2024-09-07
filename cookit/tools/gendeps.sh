#!/bin/sh

# cookit - generate binary dependency report
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

set -e

PLATFORM="$1"
shift

findso() {
    case "$PLATFORM" in
        *-apple-darwin*) NAME='*.dylib' ;;
        *-mingw32)       NAME='*.dll'   ;;
        *)               NAME='*.so'    ;;
    esac
    find "$1" -type f -name "$NAME"
}

for fn; do

    if [ -d "$fn" ]; then
        findso "$fn" | sort | while IFS= read -r line; do
            "$0" "$PLATFORM" "$line"
        done
        continue
    fi

    BASENAME="$(basename "$fn")"

    case "$PLATFORM" in
        *-apple-darwin*)
            echo "$BASENAME DEPS $(otool -l "$fn" \
                | grep -A3 'cmd LC_LOAD_DYLIB' \
                | grep 'name ' \
                | awk '{print $2}' \
                | sort \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            RPATH="$(otool -l "$fn" \
                | grep -A3 'cmd LC_RPATH' \
                | grep 'path ' \
                | sed 's/^[[:space:]]*path[[:space:]]*//' \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            [ -n "$RPATH" ] || RPATH="None"
            echo "$BASENAME RPATH $RPATH"
            ;;
        *-linux-*)
            echo "$BASENAME DEPS $(ldd "$fn" \
                | awk '{print $1}' \
                | sort \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            RPATH="$(objdump -x -j .dynamic "$fn" \
                | grep -E '^[[:space:]]*R(UN)?PATH' \
                | awk '{print $2}' \
                | sort \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            [ -n "$RPATH" ] || RPATH="None"
            echo "$BASENAME RPATH $RPATH"
            echo "$BASENAME GLIBC $(objdump -T "$fn" \
                | grep GLIBC_ \
                | sed -e 's/.*GLIBC_//' -e 's/[^[:digit:].].*//' \
                | sort --version-sort --reverse \
                | uniq \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            ;;
        *-mingw32)
            echo "$BASENAME DEPS $(objdump -p "$fn" \
                | grep 'DLL Name:' \
                | awk '{print $3}' \
                | sort \
                | tr '\n' ' ' \
                | sed 's/[[:space:]]*$//')"
            ;;
        *)
            echo "Error: unknown platform '$1'" >&2
            exit 1
            ;;
    esac

done