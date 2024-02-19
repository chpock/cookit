package require Tcl 8.4
package require tcltest 2.2
source [file join [file dirname [info script]] testutil.tcl]
load_twapi;                             # Is this needed in all.tcl?

# Test configuration options that may be set are:
#  systemmodificationok - will include tests that modify the system
#      configuration (eg. add users, share a disk etc.)

tcltest::configure -testdir [file dirname [file normalize [info script]]]
tcltest::configure -tmpdir $::env(TEMP)/twapi-test/[clock seconds]
eval tcltest::configure $argv
tcltest::runAllTests
