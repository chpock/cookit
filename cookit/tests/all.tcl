# cookit tests
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tcl 8.6-
package require tcltest 2.2

lappend auto_path [file join [file dirname [info nameofexecutable]] .. lib]

namespace import tcltest::*

if { [info exists ::env(COOKIT_GUI)] } {
    configure {*}$argv -testdir [file join [file dir [info script]] gui]
} else {
    configure {*}$argv -testdir [file dir [info script]]
}

runAllTests
