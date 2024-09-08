# cookit - initialize VFS
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

set vfs_out     [lindex $argv 0]
set vfs_content [lindex $argv 1]
set mnt "/mnt"

puts "Initialize VFS..."
puts "  content from: $vfs_content"
puts "  destination: $vfs_out"

# We need to get the default mount options for cookit (compression, smallfilesize)
lappend auto_path [file normalize [file join $vfs_content lib]]
package require cookit

::cookit::copyTclRuntime [file join $vfs_content manifest.txt] $vfs_out
