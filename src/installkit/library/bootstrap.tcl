# installkit - bootstrap
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

proc installkit_bootstrap {} {

    rename installkit_bootstrap {}

    set ::installkit::root "/installkitvfs"

    if { [catch {
        set ::installkit::cookfsindex [::cookfs::fsindex [$::installkit::cookfspages index]]
        set ::installkit::cookfshandle [::cookfs::Mount -pagesobject $::installkit::cookfspages \
            -fsindexobject $::installkit::cookfsindex -readonly \
            [info nameofexecutable] $::installkit::root]
    } err] } {
        return -code error "Error while mounting the internal filesystem: $err"
    }

    set ::installkit::wrapped [file exists [file join $::installkit::root main.tcl]]

    set libDir [file join $::installkit::root lib]

    # unset TCLLIBPATH variable so init.tcl will not try directories there.
    unset -nocomplain ::env(TCLLIBPATH)
    # unset TCL_LIBRARY variable that is used in TclpInitLibraryPath()
    unset -nocomplain ::env(TCL_LIBRARY)
    # these environment variables will be used by ::tcl::tm::Defaults
    lassign [split [info tclversion] .] vmaj vmin
    for { set n $vmin } { $n >= 0 } { incr n -1 } {
        unset -nocomplain ::env(TCL${vmaj}.${n}_TM_PATH)
    }
    # unset builtin variables for unix platform
    unset -nocomplain ::tcl_pkgPath ::tclDefaultLibrary

    # TODO: the following places can search packages/files in
    # unexpected directories:
    #
    # procedure ::tcl::tm::Defaults in tm.tcl
    # procedure tcl_findLibrary in auto.tcl
    # initial code in init.tcl injects into 'auto_path' variable:
    #     file join [file dirname [file dirname [info nameofexecutable]]] lib]
    #
    # These procedures should be overwritten to prevent files from being
    # sourced/loaded from unexpected locations.

    set ::tcl_library [file join $libDir tcl[info tclversion]]
    set ::tk_library [file join $libDir tk[info tclversion]]

}
