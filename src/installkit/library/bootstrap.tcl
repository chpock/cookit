# installkit - bootstrap
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This procedure will be called before Tcl/Tk initialization
proc ::installkit::preInit {} {

    rename ::installkit::preInit {}

    set ::installkit::root "/installkitvfs"

    if { [catch {
        set ::installkit::cookfsindex [::cookfs::fsindex [$::installkit::cookfspages index]]
        set ::installkit::cookfshandle [::cookfs::Mount \
            -pagesobject $::installkit::cookfspages \
            -fsindexobject $::installkit::cookfsindex \
            -readonly [info nameofexecutable] \
            $::installkit::root \
        ]
    } err] } {
        return -code error "Error while mounting the internal filesystem: $err"
    }

    set ::installkit::wrapped [file exists [file join $::installkit::root main.tcl]]

    if { $::installkit::wrapped } {
        set ::tcl_interactive 0
        set ::argv0 [file join $::installkit::root main.tcl]
    }

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

    set ::tcl_library [file join $libDir tcl[info tclversion]]
    set ::tk_library [file join $libDir tk[info tclversion]]

}

# This procedure will be called after Tcl/Tk initialization
proc ::installkit::postInit {} {

    rename ::installkit::postInit {}

    # init.tcl initializes ::auto_path with a directory value relative to
    # the current executable. This can be a security hole as malicious code
    # can be loaded from this directory. Let's override this variable so
    # that only a known location is used to download packages.
    set ::auto_path [list [file join $::installkit::root lib] $::tcl_library]

    # However, let's allow to load from $exename/../lib directory if
    # installkit is not an envelope for application and INSTALLKIT_TESTMODE
    # environment variable defined. This is developer mode, and we may
    # need to load packages from there.
    if { !$::installkit::wrapped && [info exists ::env(INSTALLKIT_TESTMODE)] } {
        # We assume the following tree structure:
        #   <root_dir>/bin/<our exe here>
        #   <root_dir>/lib/<package dir>
        lappend ::auto_path [file normalize [file join [info nameofexecutable] .. .. lib]]
    }

    # ::tcl::tm::Defaults adds default paths relative to current executable.
    # Let's drop these paths since we don't allow anything outside of our VFS
    # to be loaded.

    # Initialize Tcl Modules first
    ::tcl::tm::Defaults

    # Override the default paths
    set ::tcl::tm::paths [list]
    ::tcl::tm::add {*}[glob -directory \
        [file join $::installkit::root lib tcl[lindex [split [info tclversion] .] 0]] \
        -type d *]

    # Other places where Tcl can load something from unexpected locations:
    #   * tcl_findLibrary proc in auto.tcl

    # the ::installkit package should be loaded by default
    package require installkit

    # if we haven't wrapped up and not in thread, try checking out the simple commands
    if { !$::installkit::wrapped && ![info exists ::parentThread] } installkit::rawStartup

}

# -pagesize: Use 2MB for the page size so that all source files are packed on one page.
# -smallfilesize: However, if the file is larger than 512 KB, consider it a non-text file
#                 and place it on a separate page.
proc ::installkit::mountRoot { args } {
    tailcall ::vfs::cookfs::Mount -compression xz \
        -pagesize [expr { 1024 * 1024 * 2 }] -smallfilesize [expr { 1024 * 512 }] \
        {*}$args
}
