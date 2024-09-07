# cookit tests - various helper procedures
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

testConstraint threaded [::tcl::pkgconfig get threaded]
testConstraint linuxOnly [string equal $::tcl_platform(os) Linux]

if { $::tcl_platform(platform) eq "windows" } {
    package require twapi
    testConstraint elevated [expr { [twapi::get_process_elevation] eq "full" }]
    testConstraint notelevated [expr { [twapi::get_process_elevation] ne "full" }]
} {
    testConstraint elevated 0
    testConstraint notelevated 0
}

proc getfile { fn } { return [read [set f [open $fn r]]][close $f] }
