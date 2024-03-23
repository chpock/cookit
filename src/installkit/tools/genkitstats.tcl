# installkit - generate kit content statistic
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

package require installkit

proc fsize { size } {
    return [format "%8d" $size]
}

proc fratio { usize csize } {
    return [format "%.2f%%" [expr { 100.0 * $csize / $usize }]]
}

array set pindex [list]

set striplen [llength [file split $::installkit::root]]
foreach fn [::installkit::recursive_glob $::installkit::root *] {
    set fn [file join {*}[lrange [file split $fn] $striplen end]]
    set index [$::installkit::cookfsindex get $fn]
    set chklist [lindex $index 2]
    foreach { chkid - - } $chklist {
        lappend pindex($chkid) $fn
    }
}

set length [$::installkit::cookfspages length]
set fsize [file size [info nameofexecutable]]

puts "Total pages: $length"
puts ""

set size [$::installkit::cookfspages dataoffset 0]

puts "Stub    : packed: [fsize $size]"
puts ""

for { set i 0 } { $i < $length } { incr i } {
    if { $i == [expr { $length - 1 }] } {
        set csize [expr { [$::installkit::cookfspages filesize] - [$::installkit::cookfspages dataoffset $i] }]
    } {
        set csize [expr { [$::installkit::cookfspages dataoffset [expr { $i + 1 }]] - [$::installkit::cookfspages dataoffset $i] }]

    }
    set usize [string length [$::installkit::cookfspages get $i]]
    if { [info exists pindex($i)] } {
        set nfiles [llength $pindex($i)]
    } {
        set nfiles "<bootstrap>"
    }
    puts "Page [format "%2d" $i] : packed: [fsize $csize]; unpacked: [fsize $usize]; ratio: [fratio $usize $csize]; number of files: $nfiles"
}

puts ""
set csize [expr { $fsize - [$::installkit::cookfspages filesize] }]
set usize [string length [$::installkit::cookfspages index]]
puts "Index   : packed: [fsize $csize]; unpacked: [fsize $usize]; ratio: [fratio $usize $csize]"

puts ""
puts "-----------------------------------------------------------------------"
puts ""
puts "Content:"
puts ""

for { set i 0 } { $i < $length } { incr i } {
    if { ![info exists pindex($i)] } continue
    puts "Page [format "%2d" $i] : $pindex($i)"
}

exit 0
