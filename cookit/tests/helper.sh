#!/bin/sh

# cookit tests
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

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
    test-tdom)
        # Skip tests that depend on files that do not exist in the test directory
        skip schema-5.5 schema-23.1 schema-24.1
        ;;
    test-tclmtls)
        # Skip unstable tests
        skip "mtls-badssl-*"
        ;;
    test-cookit)
        case "$PLATFORM" in
            Linux*)
                # Don't check dependencies in Ubuntu24.04 environment.
                # Real linux build environment is Centos6
                if test -e /etc/lsb-release && grep -q -F DISTRIB_RELEASE=24.04 /etc/lsb-release; then
                    skip "cookit-deps-*"
                fi
            ;;
        esac
        ;;
    test-tclvfs)
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
    test-tcl)
        # cookit does not contain tzdata files and clock.test fails to
        # run with an error about the time zone ':America/Detroit' not being
        # available.
        notfile clock.test
        # the opt package is not included to cookit
        notfile opt.test
        case "$PLATFORM" in
            Windows-*)
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
