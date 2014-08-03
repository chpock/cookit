namespace eval cookit::tcllib {}

cookit::partRegister tcllib "Tcllib"

proc cookit::tcllib::retrievesource {} {
    set tempdir [cookit::fossilExport http://core.tcl.tk/tcllib]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir tcllib $version
}

proc cookit::tcllib::_copysource {} {
    if {[info exists ::env(COOKITSKIPTCLLIBCOPY)]} {
        cookit::log 3 "cookit::tcllib::_copysource: Skipping copying of Tcllib"
        return
    }
    set sourcedir [cookit::getSourceDirectory tcllib]
    catch {eval file delete -force [glob -nocomplain *]}
    cookit::writeFilelist [pwd] \
        [cookit::listAllFiles "" $sourcedir]
}

proc cookit::tcllib::parameters {version} {
    return [list \
        provides {} depends {tcl "" vfs "" cookfs ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tcllib::initialize-dynamic {version} {
}

proc cookit::tcllib::configure-dynamic {} {
    _copysource

    cookit::buildConfigure -sourcepath currentrelative -with-tcl relative \
        -mode dynamic
}

proc cookit::tcllib::build-dynamic-library {} {
}

proc cookit::tcllib::build-dynamic {} {
    catch {file delete -force ~/.critcl}

    if {[string match "solaris-*" $::cookit::platform]} {
        if {[info exists ::env(LD_LIBRARY_PATH)]} {
            set oldLDP $::env(LD_LIBRARY_PATH)
            set ::env(LD_LIBRARY_PATH) "[file join [cookit::getInstallDynamicDirectory] lib]:$::env(LD_LIBRARY_PATH)"
        }  else  {
            set ::env(LD_LIBRARY_PATH) "[file join [cookit::getInstallDynamicDirectory] lib]"
        }
    }

    set path $::env(PATH)
    set path1 [file nativename [file normalize [cookit::getToolsDirectory]]]
    set path2 [file nativename [file normalize [file join [cookit::getInstallDynamicDirectory] bin]]]

    if {$::cookit::platform == "win32-x86"} {
        set ::env(PATH) "$path1;$path2;$path"
    }  else  {
        set ::env(PATH) "$path1:$path2:$path"
    }

    set fh [open modules/base64/yencode.tcl r]
    fconfigure $fh -translation binary
    set fc [read $fh]
    close $fh
    regsub -line {^(catch \{package require crc32\})} $fc {#\1} cfc

    set fh [open modules/base64/yencode.tcl w]
    fconfigure $fh -translation binary
    puts -nonewline $fh $cfc
    close $fh

    if {[catch {
        eval cookit::buildMake critcl
    } err]} {
        set fh [open modules/base64/yencode.tcl w]
        fconfigure $fh -translation binary
        puts -nonewline $fh $fc
        close $fh

        set ::env(PATH) $path
        if {[string match "solaris-*" $::cookit::platform]} {
            if {[info exists oldLDP]} {
                set ::env(LD_LIBRARY_PATH) $oldLDP
            }  else  {
                unset ::env(LD_LIBRARY_PATH)
            }
        }
        error $err $::errorInfo $::errorCode
    }

    set fh [open modules/base64/yencode.tcl w]
    fconfigure $fh -translation binary
    puts -nonewline $fh $fc
    close $fh

    set ::env(PATH) $path
    if {[string match "solaris-*" $::cookit::platform]} {
        if {[info exists oldLDP]} {
            set ::env(LD_LIBRARY_PATH) $oldLDP
        }  else  {
            unset ::env(LD_LIBRARY_PATH)
        }
    }

    set libfiles [glob -nocomplain modules/tcllibc/*/*.so modules/tcllibc/*/*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tcllib::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

    set libfiles [glob -nocomplain modules/tcllibc/*/*.so modules/tcllibc/*/*.dylib]
    set libfile [lindex $libfiles 0]
    # remove x86_64 platform that gets created even if not needed
    if {[string match "macosx-*" $::cookit::platform] && (
        (![info exists ::env(CFLAGS)]) || (![string match "*-arch x86_64*" $::env(CFLAGS)])
    )} {
        if {[llength $libfiles] == 1} {
            cookit::log 2 "cookit::tcllib::build-dynamic: Removing x86_64 platform from $libfile..."
            file copy -force $libfile $libfile.tmp

            if {[catch {
                set cmd [list lipo -remove x86_64 -output $libfile $libfile.tmp]
                set eid [cookit::startExtCommand $cmd]
                cookit::waitForExtCommand $eid 0
            } err]} {
                cookit::log 1 "cookit::tcllib::build-dynamic: Error when running lipo - overwriting with temporary file"
                file copy -force $libfile.tmp $libfile
            }

            # fix the library being built as x86_64 and only loaded properly on 64-bit platforms
            set dirname [file dirname $libfile]
            if {[string match "*x86_64*" [file tail $dirname]]} {
                file rename $dirname [file join [file dirname $dirname] Darwin-x86]
            }

            file delete -force $libfile.tmp
        }
    }
    set critclfile modules/tcllibc/critcl.tcl
    if {[file exists $critclfile]} {
        # fix critcl::platform to report x86 on x64 machines if binary is running in 32bit mode
        # the fix is applied for all platforms in case critcl.tcl from another platform overwrites this one
        set fh [open $critclfile r]
        set fc [read $fh]
        close $fh

        regexp {^(.*proc\s+platform.*?)(return\s.*)$} $fc - before after

        if {1} {
            set fix {
                if {$mach == "x86_64" && $::tcl_platform(wordSize) == 4} {
                    set mach x86
                }
            }
        }
        # use Darwin-universal as platform for fat library
        if {[string match "macosx-universal*" $::cookit::platform]} {
            append fix {
                if {$plat == "Darwin"} {
                    return "Darwin-universal"
                }
            }
        }
        set fc $before$fix$after

        set fh [open $critclfile w]
        puts -nonewline $fh $fc
        close $fh

        # use Darwin-universal as platform for fat library - rename directory
        if {[string match "macosx-universal*" $::cookit::platform]} {
            if {[llength $libfiles] == 1} {
                set dirname [file dirname $libfile]
                set newdirname [file join [file dirname $dirname] Darwin-universal]
                cookit::log 2 "cookit::tcllib::build-dynamic: Renaming directory $dirname to $newdirname"
                if {$dirname != $newdirname} {
                    file rename $dirname $newdirname
                }
            }
        }

    }
}

proc cookit::tcllib::install-dynamic {} {
    cookit::buildMake install-libraries
}

proc cookit::tcllib::packageslist-dynamic {} {
    set filelist [list]

    set packages [list]

    set builddir [cookit::getBuildDynamicDirectory tcllib]
    set basedir [cookit::getInstallDynamicDirectory]
    set tcllibdir ""
    set tcllibcdir ""
    foreach g [glob -directory $basedir lib/tcllib*] {
        set gt [file tail $g]
        if {[regexp -nocase "tcllib(-|)\[0-9\.]+\$" $gt]} {
            set tcllibdir $g
            break
        }
    }

    if {[file exists [file join $builddir modules tcllibc]]} {
        set tcllibcdir [file join $builddir modules tcllibc]
    }

    if {$tcllibdir != ""} {
        set filelist [concat $filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/[file tail $tcllibdir]" $tcllibdir] \
            ]]
    }

    if {$tcllibcdir != ""} {
        set filelist [concat $filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/tcllibc-$::cookit::platform" $tcllibcdir] \
            ]]
    }

    if {$tcllibdir != ""} {
        lappend packages "tcllib-[string range $gt 6 end]" $filelist 
    }

    return $packages
}

