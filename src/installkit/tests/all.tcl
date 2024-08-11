# installkit tests
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

package require Tcl 8.5
package require tcltest 2.2

lappend auto_path [file join [file dirname [info nameofexecutable]] .. lib]

namespace import tcltest::*
configure {*}$argv -testdir [file dir [info script]]
runAllTests
