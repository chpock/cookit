proc installkitInit {} {
    rename installkitInit ""

    namespace eval ::installkit {
        variable root    "/installkitvfs"
        variable wrapped [file exists [file join $root main.tcl]]
    }

    package require installkit

    variable ::installkit::root

    set ::tk_library  [file join $root lib tk]
    set ::tcl_library [file join $root lib tcl]
    set ::tcl_libPath [list $::tcl_library [file join $root lib]]
    set ::auto_path   [concat $::tcl_libPath [list $::tk_library]]

    installkit::librarypath [info library]

    ## This is bad, mucking with the Tcl internals, but we don't
    ## have a command to clear out the Tcl Module search path.
    set ::tcl::tm::paths {}
    tcl::tm::path add $::tcl_library $::tk_library

    if {![info exists ::parentThread]} {
        if {![info exists ::tcl_argv]} { set ::tcl_argv $argv }
        set ::tcl_argv0 [lindex $::tcl_argv 0]
        set ::argv  [lreplace $::tcl_argv 0 0]
        set ::argv0 [lindex $::tcl_argv 0]
        if {$::installkit::wrapped} {
            set ::tcl_interactive 0
            set ::argv0 [file join $root main.tcl]
        } else {
            set ::argv0 [lindex $::argv 0]
            if {[file exists $::argv0]} {
                set ::argv [lreplace $::argv 0 0]
            } else {
                set ::argv0 $::tcl_argv0
            }
        }
        info script $::argv0
    }

    if {[info commands ::tcl_load] eq ""} {
        rename ::load ::tcl_load

        proc ::load { filename args } {
            ## Try to use the real load command and see if we can load
            ## this library.  If not, we'll try another way.
            if {![catch { eval [list ::tcl_load $filename] $args } result]} {
                return $result
            }

            ## We failed to load the library for some reason.
            ## We'll try and copy the library to a temporary
            ## location and load it from there.

            set tmpfile tcl[expr {abs([clock clicks])}][pid].tmp

            if {![info exists ::tcl_loadTempDir]} {
                ## Add a trace on the exit command to cleanup any
                ## files we extract and load on exit.
                trace add exec ::exit enter ::installkit::ExitCleanup

                set dirs {}

                if {[info exists ::tcl_TempDirs]} {
                    set dirs $::tcl_TempDirs
                }

                if {[info exists ::env(TMP)]} {
                    lappend dirs $::env(TMP)
                }

                if {[info exists ::env(TEMP)]} {
                    lappend dirs $::env(TEMP)
                }

                lappend dirs /usr/tmp /tmp /var/tmp

                foreach dir $dirs {
                    if {[file writable $dir]} {
                        set file [file join $dir $tmpfile]

                        if {[catch { file copy -force $filename $file }]} {
                            continue
                        }

                        if {$::tcl_platform(platform) ne "windows"} {
                            file attributes $file -permissions 00755
                        }

                        if {![catch { eval [list ::tcl_load $file] $args }]} {
                            lappend ::installkit::cleanupFiles $file
                            set ::tcl_loadTempDir $dir
                            break
                        } else {
                            file delete $file
                        }
                    }
                }

                if {![info exists ::tcl_loadTempDir]} {
                    return -code error "failed to load library \"$filename\":\
                        could not load file from temp location: $result"
                }
            } else {
                set file [file join $::tcl_loadTempDir $tmpfile]

                file copy -force $filename $file
                if {$::tcl_platform(platform) ne "windows"} {
                    file attributes $file -permissions 00755
                }

                lappend ::installkit::cleanupFiles $file

                return [eval [list ::tcl_load $file] $args]
            }
        }
    }

    package require miniarc
}

installkitInit
