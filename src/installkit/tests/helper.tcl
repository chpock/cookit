# installkit tests - various helper procedures
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

testConstraint threaded [::tcl::pkgconfig get threaded]

proc getfile { fn } { return [read [set f [open $fn r]]][close $f] }
