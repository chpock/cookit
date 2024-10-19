# cookit - generate kit content statistic
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit

proc ::cookit::stats { args } {

    if { [llength $args] } {
        set root    [lindex $args 0]
        ::cookfs::Mount -readonly $root $root
    } {
        set root    $::cookit::root
    }

    set frmsize [list apply { { size } {
        return [format "%8d" $size]
    } }]

    set frmratio [list apply { { usize csize } {
        return [format "%.2f%%" [expr { 100.0 * $csize / $usize }]]
    } }]

    array set pindex [list]

    foreach fn [recursive_glob $root *] {
        foreach block [file attributes $fn -blocks] {
            lappend pindex([dict get $block page]) [file attributes $fn -relative]
        }
    }

    set length [file attribute $root -pages]

    puts "Total pages: $length"
    puts ""

    set size [dict get [file attributes $root -parts] headsize]

    puts "Stub    : packed: [{*}$frmsize $size]"
    puts ""

    for { set i 0 } { $i < $length } { incr i } {

        set page [file attributes $root -pages $i]
        set csize [dict get $page compsize]
        set usize [dict get $page uncompsize]
        set comp  [dict get $page compression]

        if { [info exists pindex($i)] } {
            set nfiles [llength $pindex($i)]
        } {
            set nfiles "<bootstrap>"
        }
        puts "Page [format "%2d" $i] : packed($comp): [{*}$frmsize $csize]; unpacked: [{*}$frmsize $usize]; ratio: [{*}$frmratio $usize $csize]; number of files: $nfiles"
    }

    puts ""

    set page [file attributes $root -pages pgindex]
    set csize [dict get $page compsize]
    set usize [dict get $page uncompsize]
    set comp  [dict get $page compression]
    puts "PgIndex : packed($comp): [{*}$frmsize $csize]; unpacked: [{*}$frmsize $usize]; ratio: [{*}$frmratio $usize $csize]"

    set page [file attributes $root -pages fsindex]
    set csize [dict get $page compsize]
    set usize [dict get $page uncompsize]
    set comp  [dict get $page compression]
    puts "FsIndex : packed($comp): [{*}$frmsize $csize]; unpacked: [{*}$frmsize $usize]; ratio: [{*}$frmratio $usize $csize]"

    puts ""
    puts "-----------------------------------------------------------------------"
    puts ""
    puts "Content:"
    puts ""

    for { set i 0 } { $i < $length } { incr i } {
        if { ![info exists pindex($i)] } continue
        puts "Page [format "%2d" $i] : $pindex($i)"
    }

}

package provide cookit::stats 1.0.0
