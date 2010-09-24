namespace eval cookit::cookit {}

cookit::partRegister -versions 1.0 cookit "CooKit Core"

proc cookit::cookit::parameters {version} {
    set dep [list tcl {} zlib {} vfs {} cookfs {}]
    if {$::cookit::platform == "win32-x86"} {
        lappend dep resources ""
    }
    return [list \
        provides {TARGET 1.0} depends $dep \
        buildmodes {static} \
        ]
}

proc cookit::cookit::initialize-static {version} {
}

proc cookit::cookit::configure-static {} {
}

proc cookit::cookit::build-static {} {
    set gcc [cookit::osSpecific gcc]

    set compileflags [cookit::getParameters cookitCompileFlags]
    set command $gcc
    lappend command -I.
    lappend command -I[cookit::wdrelative [file join [cookit::getSourceDirectory tcl] generic]]
    lappend command -I[cookit::wdrelative [file join [cookit::getSourceDirectory tcl] $::cookit::unixwinplatform]]
    
    lappend command -DTCL_LOCAL_APPINIT=CooKit_AppInit
    lappend command -DSTATIC_BUILD=1
    lappend command -DBUILD_tcl
    if {[cookit::isPartIncluded tk]} {
        set appinitdir [file join [cookit::getSourceDirectory tk] unix]
        set appinitfile tkAppInit
        lappend command -DTK_LOCAL_APPINIT=CooKit_AppInit
        lappend command -DBUILD_tk
    }  else  {
        set appinitdir [file join [cookit::getSourceDirectory tcl] $::cookit::unixwinplatform]
        set appinitfile tclAppInit
    }

    set command [concat $command $compileflags]

    set sfh [open [file join [cookit::getSourceDirectory cookit] cookit.c] r]
    set dfh [open cookit.c w]

    set fc [read $sfh]
    puts -nonewline $dfh [string map [list \
        "/* %COOKIT_INCLUDE_CODE% */" [cookit::getParameters cookitIncludeCode] \
        "/* %COOKIT_EXTERN_CODE% */" [cookit::getParameters cookitExternCode] \
        "/* %COOKIT_APPINIT_CODE% */" [cookit::getParameters cookitAppinitCode] \
        ] $fc]

    close $sfh
    close $dfh

    foreach {sourcedir file destfile} [list \
        . cookit cookit \
        $appinitdir $appinitfile appInit \
        ] {
        set eid [cookit::startExtCommand [linsert $command end \
            -c -o $destfile.o \
            [cookit::wdrelative [file join $sourcedir $file.c]]] \
            ]
        cookit::waitForExtCommand $eid 0
    }
}

proc cookit::cookit::install-static {} {
}

proc cookit::cookit::vfsfilelist-static {} {
    set filelist [list]
    set src [cookit::getSourceDirectory cookit]
    return [list]
}

proc cookit::cookit::linkerflags-static {} {
    set rc [list]
    if {$::cookit::platform == "win32-x86"} {
        # this will get overwritten by Tk, if needed
        lappend rc "-mconsole"
    }
    return $rc
}

proc cookit::cookit::linkerfiles-static {} {
    set rc [list]
    lappend rc [cookit::wdrelative [file join [cookit::getBuildStaticDirectory cookit] cookit.o]]
    lappend rc [cookit::wdrelative [file join [cookit::getBuildStaticDirectory cookit] appInit.o]]
    return $rc
}


proc cookit::cookit::vfsbootstrap {} {
    set code ""
    
    set src [cookit::getSourceDirectory cookit]
    set cfssrc [cookit::getSourceDirectory cookfs]

    foreach g [concat \
        [list [file join $src lib cmdline.tcl]] \
        [list [file join $cfssrc scripts cookvfs.tcl]] \
        [list [file join $cfssrc scripts memchan.tcl]] \
        [list [file join $cfssrc scripts readerchannel.tcl]] \
        [list [file join $cfssrc scripts vfs.tcl]] \
        [list [file join $cfssrc scripts writer.tcl]] \
        ] {
        set fh [open $g r]
        fconfigure $fh -translation binary
        set data [read $fh]
        close $fh

        regsub -all -line -- "package require vfs" $data "" data
        regsub -all -line -- "^#.*\$" $data "" data
        regsub -all -line -- "^ +" $data "" data
        regsub -all -line -- "\r+" $data "" data
        regsub -all -line -- "\n+" $data "\n" data
        
        append code $data \n
    }

    append code {set ::cookit::cookitfsid [cookfs::Mount -readonly -pagesobject $::cookit::cookitpages [info nameofexecutable] [info nameofexecutable]]} \n
    append code [list set ::cookit::cookitplatform $::cookit::platform] \n
    append code "set sourcevfs \[list source \[file join \[info nameofexecutable\] lib [list vfs[cookit::getPartVersion vfs]] vfs.tcl\]\]\n"
    append code {
        if {[catch {
            set ::cookit::cookitlibpath [file join [info nameofexecutable] lib]
            set ::tcl_library [file join $::cookit::cookitlibpath tcl[info tclversion]]
            set ::tcl_pkgPath [list $::cookit::cookitlibpath]
            set ::tcl_libPath [list $cookit::cookitlibpath $::tcl_library]

            set ::auto_path $::tcl_libPath
            source $::tcl_library/init.tcl
            set ::auto_path $::tcl_libPath

            eval $sourcevfs
            unset sourcevfs

            namespace eval ::cookit {}

            proc ::cookit::writetomemory {} {cookfs::writetomemory $::cookit::cookitfsid}
            proc ::cookit::aside {filename} {cookfs::aside $::cookit::cookitfsid $filename}

            proc _cookit_dumpinitcode {} {
                rename _cookit_dumpinitcode ""
                set rc ""
                foreach var {tcl_library tcl_libPath auto_path} {
                    append rc [list set ::$var [set ::$var]] \n
                }
                return $rc
            }

            proc _cookit_handlemainscript {} {
                global argc argv argv0 tcl_interactive
                rename _cookit_handlemainscript ""

                if {[file isfile [file join [info nameofexecutable] main.tcl]]} {
                    if {[info commands console] != {}} { console hide }
                    set tcl_interactive 0
                    incr argc
                    set argv [linsert $argv 0 $argv0]
                    set argv0 [file join [info nameofexecutable] main.tcl]
                    return $argv0
                }  else  {
                    return ""
                }
            }
        }]} {
            puts "Initialization error:\n$::errorInfo"
        }

    }

    return $code
}
