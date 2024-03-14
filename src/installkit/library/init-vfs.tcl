# installkit - initialize VFS
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

set bootstrap_script "bootstrap-vfs.tcl"
set vfs_out     [lindex $argv 0]
set vfs_content [lindex $argv 1]
set mnt "/mnt"

puts "Initialize VFS..."
puts "  bootstrap script: $bootstrap_script"
puts "  content from: $vfs_content"
puts "  destination: $vfs_out"

source $bootstrap_script

set fp [open $bootstrap_script r]
fconfigure $fp -encoding binary -translation binary
set bootstrap [read $fp]
close $fp

proc addFilesFrom { dir { level 0 } } {
    if { ![info exists ::strip_count] } {
        set ::strip_count [llength [file split $::vfs_content]]
    }
    foreach file [glob -nocomplain -directory $dir -type f *] {
        set dst [file split $file]
        set dst [lrange $dst $::strip_count end]
        set dst [file join $::mnt {*}$dst]
        set dstDir [file dirname $dst]
        if { ![file isdirectory $dstDir] } {
            file mkdir $dstDir
        }
        puts "    [string repeat {  } $level]add file: $dst"
        file copy $file $dst
    }
    foreach subdir [glob -nocomplain -directory $dir -type d *] {
        puts "    [string repeat {  } $level]add directory: $subdir"
        addFilesFrom $subdir [expr { $level + 1 }]
    }
}

vfs::cookfs::Mount -bootstrap $bootstrap -compression xz $vfs_out $mnt
addFilesFrom $vfs_content
vfs::unmount $mnt


