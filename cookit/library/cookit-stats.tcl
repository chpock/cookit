# cookit - generate kit content statistic
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit

proc ::cookit::stats { args } {

    if { [llength $args] } {
        set fname   [lindex $args 0]
        set fsize   [file size $fname]
        set root    $fname
        set pages   [::cookfs::pages -readonly $fname]
        set fsindex [::cookfs::fsindex [$pages index]]
        ::cookfs::Mount -pagesobject $pages -fsindexobject $fsindex \
            -readonly $fname $root
    } {
        set root    $::cookit::root
        set fsindex [[file attributes $::cookit::root -handle] getindex]
        set pages   [[file attributes $::cookit::root -handle] getpages]
        set fsize   [file size [info nameofexecutable]]
    }

    set frmsize [list apply { { size } {
        return [format "%8d" $size]
    } }]

    set frmratio [list apply { { usize csize } {
        return [format "%.2f%%" [expr { 100.0 * $csize / $usize }]]
    } }]

    array set pindex [list]

    set striplen [llength [file split $root]]
    foreach fn [recursive_glob $root *] {
        set fn [file join {*}[lrange [file split $fn] $striplen end]]
        set index [$fsindex get $fn]
        set chklist [lindex $index 2]
        foreach { chkid - - } $chklist {
            lappend pindex($chkid) $fn
        }
    }

    set length [$pages length]

    puts "Total pages: $length"
    puts ""

    set size [$pages dataoffset 0]

    puts "Stub    : packed: [{*}$frmsize $size]"
    puts ""

    for { set i 0 } { $i < $length } { incr i } {
        if { $i == [expr { $length - 1 }] } {
            set csize [expr { [$pages filesize] - [$pages dataoffset $i] }]
        } {
            set csize [expr { [$pages dataoffset [expr { $i + 1 }]] - [$pages dataoffset $i] }]
        }
        set usize [string length [$pages get $i]]
        if { [info exists pindex($i)] } {
            set nfiles [llength $pindex($i)]
        } {
            set nfiles "<bootstrap>"
        }
        puts "Page [format "%2d" $i] : packed: [{*}$frmsize $csize]; unpacked: [{*}$frmsize $usize]; ratio: [{*}$frmratio $usize $csize]; number of files: $nfiles"
    }

    puts ""
    set csize [expr { $fsize - [$pages filesize] }]
    set usize [string length [$pages index]]
    puts "Index   : packed: [{*}$frmsize $csize]; unpacked: [{*}$frmsize $usize]; ratio: [{*}$frmratio $usize $csize]"

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
