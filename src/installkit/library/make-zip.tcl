# installkit - make zip archive
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

package require installkit
package require vfs::wzip

vfs::zip::Mount [lindex $argv 0] [set mnt [::installkit::tmpmount]] -readwrite
file copy {*}[lrange $argv 1 end] $mnt
vfs::unmount $mnt
