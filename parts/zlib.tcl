namespace eval cookit::zlib {}

cookit::partRegister zlib "Zlib for Tcl 8.5"

proc cookit::zlib::_copysource {} {
    if {[info exists ::env(COOKITSKIPZLIBCOPY)]} {
        cookit::log 3 "cookit::zlib::_copysource: Skipping copying of zlib"
        return
    }
    set sourcedir [cookit::getSourceDirectory zlib]
    catch {eval file delete -force [glob -nocomplain *]}
    cookit::writeFilelist [pwd] \
        [cookit::listAllFiles "" $sourcedir]
}

proc cookit::zlib::retrievesource {} {
    set html [cookit::downloadURL http://zlib.net/]
    if {![regexp -nocase {<a href="(http://zlib.net/zlib-[0-9\.]+.tar.gz)"} $html - fileurl]} {
        error "Unable to retrieve URL to archive"
    }

    set destfile [file join [cookit::getDownloadsDirectory] [file tail $fileurl]]
    cookit::downloadURL $fileurl $destfile
}

proc cookit::zlib::parameters {version} {
    return [list \
        provides {} depends {} \
        buildmodes {static} \
        ]
}

proc cookit::zlib::initialize-static {version} {
}

proc cookit::zlib::configure-static {} {
    cookit::log 3 "cookit::zlib::configure-static: Syncing up zlib sources"
    _copysource

    if {$::cookit::platform != "win32-x86"} {
        set d [cookit::setEnv CC [cookit::osSpecific gcc]]
        set additional {}

        if {$::cookit::opt(64bit)} {
            lappend additional --64
        }

        cookit::log 3 "cookit::zlib::configure-static: Configuring zlib with [list $additional] additional parameters"
        if {[catch {
            cookit::buildConfigure -always -sourcepath relative -mode static -skipthreads -skipshared -skip64bit \
                -additional $additional
        } err]} {
            set ei $::errorInfo
            set ec $::errorCode
            cookit::revertEnv $d
            error $err $ei $ec
        }
        cookit::revertEnv $d
    }  else  {
        cookit::log 3 "cookit::zlib::configure-static: Not configuring zlib - using win32/Makefile.gcc"
    }
}

proc cookit::zlib::build-static {} {
    set d [cookit::setEnv CC [cookit::osSpecific gcc]]
    if {[catch {
        if {$::cookit::platform == "win32-x86"} {
            cookit::buildMake -f win32/Makefile.gcc libz.a
        }  else  {
            cookit::buildMake libz.a
        }
    } err]} {
        set ei $::errorInfo
        set ec $::errorCode
        cookit::revertEnv $d
        error $err $ei $ec
    }
    cookit::revertEnv $d
}

proc cookit::zlib::install-static {} {
    # do nothing since install target also builds *.so which is not needed
}

proc cookit::zlib::vfsfilelist-static {} {
    set filelist [list]
    return $filelist
}

proc cookit::zlib::linkerfiles-static {} {
    set rc [list]
    #lappend rc [file join [cookit::getBuildStaticDirectory zlib] libz.a]
    return $rc
}

proc cookit::zlib::linkerflags-static {} {
    set rc [list]
    #lappend rc [file join [cookit::getBuildStaticDirectory zlib] libz.a]
    return $rc
}
