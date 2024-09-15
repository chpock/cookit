# cookit - builtin commands
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit

namespace eval ::cookit::builtin {}

proc ::cookit::builtin::run { } {
    set ::tcl_interactive 0
    set cmd [lindex $::argv 0]
    switch -exact -- $cmd {
        --wrap {
            ::cookit::wrap {*}[lrange $::argv 1 end]
        }
        --stats {
            package require cookit::stats
            ::cookit::stats {*}[lrange $::argv 1 end]
        }
        default {
            puts stderr "Error: unknown command '$cmd'"
            puts stderr "Known commands are: --wrap, --stats"
            exit 1
        }
    }
    exit 0
}

package provide cookit::builtin 1.0.0
