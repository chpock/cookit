namespace eval cookit::cookit {}

cookit::partRegister -versions 1.0 cookit "CooKit Core"

proc cookit::cookit::parameters {version} {
    # Removed zlib as we can build 8.5 using bzip2
    set dep [list tcl {} vfs {} cookfs {} memchan {}]
    if {$::cookit::platform == "win32-x86"} {
        lappend dep resources ""
    }
    # this might actually break - if it does, just put if {1} and
    # later check when adding files to link against
    if {[string match "8.5.*" [cookit::getPartVersion tcl]]} {
        lappend dep zlib ""
    }
    return [list \
        provides {virtual:cookit 1.0} depends $dep \
        buildmodes {static} \
        ]
}

proc cookit::cookit::initialize-static {version} {
}

proc cookit::cookit::configure-static {} {
}

proc cookit::cookit::build-static {} {
    cookit::loadConfigFile tcl conf

    # prepare filelist for compiling and code
    set filelist [list \
        . cookit cookit \
        ] 

    # build cookit.c and compile it
    set gcc [cookit::osSpecific gcc]

    set compileflags [cookit::getParameters cookitCompileFlags]
    set command $gcc
    if {[info exists ::env(CFLAGS)]} {
        set command [concat $gcc $::env(CFLAGS)]
    }
    set command [concat $command $conf(TCL_EXTRA_CFLAGS)]

    lappend command -I.
    lappend command -I[cookit::wdrelative [file join [cookit::getSourceDirectory tcl] generic]]
    lappend command -I[cookit::wdrelative [file join [cookit::getSourceDirectory tcl] $::cookit::unixwinplatform]]

    lappend command -DTCL_LOCAL_APPINIT=CooKit_AppInit
    lappend command -DSTATIC_BUILD=1
    lappend command -DBUILD_tcl
    lappend command -DMODULE_SCOPE=extern
    if {[cookit::isPartIncluded tk] && ($::cookit::platform == "win32-x86")} {
        # rewrite tkAppInit removing #undef that causes the build to crash
        set appinitdir [file join [cookit::getSourceDirectory tk] unix]
        set fh [open [file join $appinitdir tkAppInit.c] r]
        fconfigure $fh -translation auto -encoding iso8859-1
        set fc [read $fh]
        close $fh

        set fc [string map [list "#undef BUILD_tk" "" "#undef STATIC_BUILD" ""] $fc]
        set fh [open tkAppInit.c w]
        puts $fh $fc
        close $fh

        set appinitdir .
        set appinitfile tkAppInit
        lappend command -DTK_LOCAL_APPINIT=CooKit_AppInit
        lappend command -DBUILD_tk
    }  else  {
        set appinitdir [file join [cookit::getSourceDirectory tcl] $::cookit::unixwinplatform]
        set appinitfile tclAppInit
    }

    if {[string match "8.5.*" [cookit::getPartVersion tcl]]} {
        lappend filelist [cookit::getSourceDirectory cookit] tclzlib tclzlib
        cookit::addParameters cookitExternCode "Tcl_AppInitProc Tclzlib_Init;\n"
        cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"zlib\", Tclzlib_Init, NULL);\n"
	lappend command -DCOOKIT_STATIC_ZLIB -I[cookit::getBuildStaticDirectory zlib]
    }

    lappend filelist $appinitdir $appinitfile appInit

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

    foreach {sourcedir file destfile} $filelist {
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
    if {[string match "8.5.*" [cookit::getPartVersion tcl]]} {
        lappend rc [cookit::wdrelative [file join [cookit::getBuildStaticDirectory cookit] tclzlib.o]]
        lappend rc [cookit::wdrelative [file join [cookit::getBuildStaticDirectory zlib] libz.a]]
    }
    return $rc
}


proc cookit::cookit::vfsbootstrap {} {
    set code ""
    
    set src [cookit::getSourceDirectory cookit]
    set cfssrc [cookit::getSourceDirectory cookfs]

    set cookfsVersion [cookit::getPartVersion cookfs] 

    set filelist [list]

    if {[string equal $cookfsVersion "1.0"]} {
	lappend filelist [file join $src lib cmdline.tcl]
    }  else  {
	lappend filelist [file join [cookit::getBuildDirectory cookfs] pkgconfig.tcl]
    }

    lappend filelist \
        [file join $cfssrc scripts memchan.tcl] \
        [file join $cfssrc scripts readerchannel.tcl] \
        [file join $cfssrc scripts vfs.tcl] \
        [file join $cfssrc scripts writer.tcl]

    if {[file exists [file join $cfssrc scripts optimize.tcl]]} {
	lappend filelist \
	    [file join $cfssrc scripts optimize.tcl]
    }

    lappend filelist \
        [file join $cfssrc scripts cookvfs.tcl]

    foreach g $filelist {
        set fh [open $g r]
        fconfigure $fh -translation binary
        set data [read $fh]
        close $fh

        regsub -all -line -- "package require vfs\\s*\$" $data "" data
        regsub -all -line -- "^#.*\$" $data "" data
        regsub -all -line -- "^ +" $data "" data
        regsub -all -line -- "\r+" $data "" data
        regsub -all -line -- "\n+" $data "\n" data
        
        append code $data \n
    }

    # initialize fsindex
    append code {set ::cookit::cookitfsindex [cookfs::fsindex [$::cookit::cookitpages index]]} \n
    # get metadata
    append code {set ::cookit::cookitmountpoint [subst [$::cookit::cookitfsindex getmetadata cookit.mountpoint {[info nameofexecutable]}]]} \n

    # mount cookfs archive
    append code {
	set ::cookit::cookfshandle [eval [concat \
	    [list cookfs::Mount -pagesobject $::cookit::cookitpages -fsindexobject $::cookit::cookitfsindex -readonly] \
	    [$::cookit::cookitfsindex getmetadata cookit.mountflags {}] \
	    [list [info nameofexecutable] $::cookit::cookitmountpoint] \
	    ]]
    }
    
    # set platform
    append code [list set ::cookit::cookitplatform $::cookit::platform] \n
    append code "set sourcevfs \[list source \[file join \$::cookit::cookitmountpoint lib [list vfs[cookit::getPartVersion vfs]] vfs.tcl\]\]\\\;\[list source \[file join \$::cookit::cookitmountpoint lib [list vfs[cookit::getPartVersion vfs]] vfslib.tcl\]\]\n"
    append code {
        if {[catch {
            set ::cookit::cookitlibpath [file join $::cookit::cookitmountpoint lib]
            set ::tcl_library [file join $::cookit::cookitlibpath tcl[info tclversion]]
            set ::tcl_pkgPath [list $::cookit::cookitlibpath]
            set ::tcl_libPath [list $cookit::cookitlibpath $::tcl_library]

            set ::auto_path $::tcl_libPath
            source $::tcl_library/init.tcl
            set ::auto_path $::tcl_libPath

            eval $sourcevfs
            unset sourcevfs

            namespace eval ::cookit {}

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

                if {[file isfile [file join $::cookit::cookitmountpoint main.tcl]]} {
                    if {[info commands console] != {}} { console hide }
                    set tcl_interactive 0
                    incr argc
                    set argv [linsert $argv 0 $argv0]
                    set argv0 [file join $::cookit::cookitmountpoint main.tcl]
                    return $argv0
                }  else  {
                    return ""
                }
            }
        }]} {
            puts "[$::cookit::cookitfsindex getmetadata cookit.initerrormessage {Cookit initialization error}]:\n$::errorInfo"
        }

    }

    return $code
}
