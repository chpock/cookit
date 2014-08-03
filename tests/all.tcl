package require tcltest

puts "Platform:"
parray tcl_platform

if {[info exists ::env(COOKITPKGS)] && ($::env(COOKITPKGS) == "1" || $::env(COOKITPKGS) == "yes" || $::env(COOKITPKGS) == "true")} {
    set d [file normalize [file dirname [info nameofexecutable]]]
    foreach g [glob -directory $d packages/*.cfspkg] {
        vfs::cookfs::Mount $g $g -readonly
        lappend auto_path $g/lib
        tcl::tm::path add $g/lib
    }
}

set tmpdir [file normalize [file join [file dirname [info script]] _tests]]
catch {file delete -force $tmpdir}
file mkdir $tmpdir

if {![file writable $tmpdir]} {
    set tmpdir [file normalize $::env(TEMP)/_cookit_tests]
    catch {file delete -force $tmpdir}
    file mkdir $tmpdir
}

tcltest::temporaryDirectory $tmpdir
tcltest::testsDirectory [file dirname [info script]]

if {[info tclversion] == "8.4"} {
    package require rechan
}

set constraints [tcltest::configure -constraints]

tcltest::configure -constraints $constraints

puts stdout "Tests running in interp:  [info nameofexecutable]"
puts stdout "Tests running in working dir:  $::tcltest::testsDirectory"
if {[llength $::tcltest::skip] > 0} {
    puts stdout "Skipping tests that match:  $::tcltest::skip"
}
if {[llength $::tcltest::match] > 0} {
    puts stdout "Only running tests that match:  $::tcltest::match"
}

if {[llength $::tcltest::skipFiles] > 0} {
    puts stdout "Skipping test files that match:  $::tcltest::skipFiles"
}
if {[llength $::tcltest::matchFiles] > 0} {
    puts stdout "Only sourcing test files that match:  $::tcltest::matchFiles"
}

set timeCmd {clock format [clock seconds]}
puts stdout "Tests began at [eval $timeCmd]"

# source each of the specified tests
foreach file [lsort [::tcltest::getMatchingFiles]] {
    set tail [file tail $file]
    puts stdout $tail
    if {[catch {source $file} msg]} {
	puts stdout $msg
    }
}

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1
return

