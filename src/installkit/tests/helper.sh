#!/bin/sh

# installkit tests
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

skip() {
    for i; do
        [ -z "$SKIP" ] || SKIP="$SKIP "
        SKIP="$SKIP$i"
    done
}

notfile() {
    for i; do
        [ -z "$NOTFILE" ] || NOTFILE="$NOTFILE "
        NOTFILE="$NOTFILE$i"
    done
}

case "$1" in
    check-installkit)
        case "$PLATFORM" in
            Linux*)
                # Don't check dependencies in Ubuntu24.04 environment.
                # Real linux build environment is Centos6
                if test -e /etc/lsb-release && grep -q -F DISTRIB_RELEASE=24.04 /etc/lsb-release; then
                    skip "installkit-deps-*"
                fi
            ;;
        esac
        ;;
    check-tclvfs)
        # Tests 4.1 and 4.2 depend on vfs::ns, which we don't ship
        skip vfs-4.1 vfs-4.2
        case "$PLATFORM" in
            Linux-*)
                # vfsTar-1.2 - attempt to delete mounted file is successful on Linux
                skip vfsTar-1.2
                # vfsZip-9.0 - attempt to delete mounted file is successful on Linux
                skip vfsZip-9.0
            ;;
            MacOS-X)
                # vfsTar-1.2 - attempt to delete mounted file is successful on Linux
                skip vfsTar-1.2
                # vfsZip-9.0 - attempt to delete mounted file is successful on Linux
                skip vfsZip-9.0
            ;;
        esac
        ;;
    check-tcl)
        # installkit does not contain tzdata files and clock.test fails to
        # run with an error about the time zone ':America/Detroit' not being
        # available.
        notfile clock.test
        # the opt package is not included to installkit
        notfile opt.test
        # This test depends on 'shiftjis' encoding which is not in installkit. See
        # installkit test 'encoding' in test file 'content.test'.
        skip chan-io-1.7 "chan-io-7.*" "io-7.*"
        # These test depends on 'iso2022-jp' endocing. It exists, but it is broken in IJ 1.3.0.
        # TODO: think what to do with broken iso2022-jp
        skip chan-io-1.8 chan-io-1.9 chan-io-6.55
        skip encoding-11.5 encoding-11.5.* encoding-13.1 encoding-21.1 encoding-22.1 "encoding-23.*"
        skip "encoding-24.*"
        skip io-1.8 io-1.9 io-6.55
        case "$PLATFORM" in
            Windows)
                # These tests call the interpreter itself, and they get stuck
                # because of Tk.
                skip compile-13.1 event-7.5
                # This test raises an expected error. But it appears as GUI window on Windows.
                skip "basic-46.*" event-7.5
                skip subst-5.8 subst-5.9 subst-5.10
                skip tcltest-14.1 tcltest-22.1
                skip "winpipe-4.*"
                # These tests run the interpreter with a script that has no exit or return.
                # That enters into Tk event loop, and interpreter doesn't terminate.
                skip chan-io-12.5 chan-io-29.21 chan-io-29.22 chan-io-29.23 chan-io-29.33
                skip chan-io-32.10 chan-io-32.11 chan-io-33.3 chan-io-39.10 chan-io-53.9
                skip exec-9.8 exec-11.5 exec-16.2 exec-17.1 regexp-14.3 regexpComp-14.3
                skip winpipe-5.1
                # These tests are for tclsh. They expect the interpreter to run a shell script.
                # But Tk has a different behaviour for unknown commands.
                skip exec-4.2 exec-4.3 exec-4.5 "exec-6.*" exec-9.6 exec-9.7 exec-14.5 "exec-15.*"
                # On Windows the Tk_Main is used, not Tcl_Main. All of these tests are
                # expected to fail.
                notfile main.test
                # This test file contains unknown command that raises GUI error on Windows.
                notfile safe.test
                # These tests depend on event loop and don't work as expected in Tk
                skip chan-io-53.8
                # These tests expects error on stdout from interpreter. They doesn't work with Tk.
                notfile stack.test
                # These tests expects error on stdout from interpreter. They doesn't work with Tk.
                skip "winpipe-8.*"
                # These tests pass command line parameters that are intercepted by tk_Main
                skip "tcltest-1.*"
                # These tests are failed due to unknown error.
                # TODO: Investigate the error and check if installkit 1.3.0 is affected also.
                skip chan-io-14.3 chan-io-14.4 chan-io-20.5 chan-io-34.8 chan-io-34.16
                skip chan-io-34.17 chan-io-35.2 chan-io-35.3 chan-io-53.7 chan-io-53.8a
                ;;
        esac
    ;;
esac

set --
[ -z "$SKIP" ] || set -- "$@" "-skip" "\"$SKIP\""
[ -z "$NOTFILE" ] || set -- "$@" "-notfile" "\"$NOTFILE\""

echo "$@"
#[ -z "$SKIP" ] || echo "-skip" "'$SKIP'"
#[ -z "$SKIP" ] || echo "-skip" "$(printf '%q' "$SKIP")"
