# cookit - make zip archive
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit
package require vfs::wzip

set archive [lindex $argv 0]
vfs::zip::Mount $archive $archive -readwrite
file copy {*}[lrange $argv 1 end] $archive
vfs::unmount $archive
