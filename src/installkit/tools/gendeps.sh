#!/bin/sh

# installkit - generate binary dependency report
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

case "$1" in
    *-apple-darwin*)
        otool -L "$2" | tail -n+2 | awk '{print $1}' | sort
        ;;
    *-linux-*)
        ldd "$2" | awk '{print $1}' | sort
        ;;
    *-mingw32)
        objdump -p "$2" | grep 'DLL Name:' | awk '{print $3}' | sort
        ;;
    *)
        echo "Error: unknown platform '$1'" >&2
        exit 1
        ;;
esac