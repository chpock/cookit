# installkit - fake ffidl package
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# In installkit v1.4.0 we don't build ffidl package. However,
# InstallJammer v1.3.0 expects it to be present in the installkit.
# Only one SHChangeNotify procedure is used in InstallJammer from ffidl.
# Here we emulate this package and this function using twapi.

package require twapi

namespace eval ::ffidl {}

proc ::ffidl::SHChangeNotify { wEventId uFlags dwItem1 dwItem2 } {
    tailcall ::twapi::SHChangeNotify $wEventId $uFlags $dwItem1 $dwItem2
}

package provide Ffidl 0.8
