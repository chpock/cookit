# cookit - builtin commands
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit

namespace eval ::cookit::builtin {}

proc ::cookit::builtin::wrap { args } {

    if { ![llength $args] } {
        puts stderr "Error: the main file is not specified"
        puts stderr "Usage: --wrap script_file.tcl ?options?"
        exit 1
    }

    set known_options [list {*}{
        --paths --path --to --as
        --output --stubfile --compression
        --icon --company --copyright --fileversion --productname
        --productversion --filedescription --originalfilename
    }]

    set main_file [lindex $args 0]
    set options   [list]

    for { set i 1 } { $i < [llength $args] } { incr i } {

        set arg [lindex $args $i]

        if { $arg ni $known_options } {
            puts stderr "Error: unknown option \"$arg\""
            puts stderr "Known options are: [join [lrange $known_options 0 end-1] {, }] and [lindex $known_options end]"
            exit 1
        }

        if { [incr i] == [llength $args] } {
            return -code error "missing value for argument '$arg'"
        }
        set val [lindex $args $i]

        if { $arg eq "--paths" } {
            set val [list $val]
            while { [incr i] < [llength $args] && ![string match {--*} [lindex $args $i]] } {
                lappend val [lindex $args $i]
            }
            # go back one argument if we have not reached the end of the arguments
            # but have found another option
            if { $i < [llength $args] } {
                incr i -1
            }

        }

        lappend options [string range $arg 1 end] $val

    }

    tailcall ::cookit::wrap $main_file {*}$options

}

proc ::cookit::builtin::run { } {
    if { [catch {
        set ::tcl_interactive 0
        set cmd [lindex $::argv 0]
        switch -exact -- $cmd {
            --wrap {
                wrap {*}[lrange $::argv 1 end]
            }
            --stats {
                package require cookit::stats
                ::cookit::stats {*}[lrange $::argv 1 end]
            }
            --version {
                set message "Cookit version [::cookit::pkgconfig get package-version]"
                if { [catch { package require Tk }] } {
                    puts stdout $message
                } else {
                    # hide toplevel window
                    wm withdraw .
                    tk_messageBox -message $message -title "Cookit about" -type ok -icon info
                }
            }
            --upgrade - --update {
                package require cookit::install
                ::cookit::install::run upgrade {*}[lrange $::argv 1 end]
            }
            --check-upgrade - --check-update {
                package require cookit::install
                ::cookit::install::run check-upgrade {*}[lrange $::argv 1 end]
            }
            --uninstall {
                package require cookit::install
                ::cookit::install::uninstall {*}[lrange $::argv 1 end]
            }
            --install {
                package require cookit::install
                ::cookit::install::run install {*}[lrange $::argv 1 end]
            }
            default {
                puts stderr "Error: unknown command '$cmd'"
                puts stderr "Known commands are: --wrap, --stats, --version, --install, --check-upgrade, --upgrade and --uninstall"
                exit 1
            }
        }
    } err] } {
        puts "Internal error: $err"
        puts $::errorInfo
        exit 1
    }
    exit 0
}

package provide cookit::builtin 1.0.0
