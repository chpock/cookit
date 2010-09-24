namespace eval cookit::thread {}

cookit::partRegister thread "Tcl threads support"

proc cookit::thread::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tcl.cvs.sourceforge.net:/cvsroot/tcl thread]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir thread $version
}

proc cookit::thread::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
	buildmodes {static} \
        ]
}

proc cookit::thread::initialize-static {version} {
}

proc cookit::thread::configure-static {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative
}

proc cookit::thread::build-static {} {
    cookit::buildMake all
}

proc cookit::thread::install-static {} {
    # this is not needed
    cookit::buildMake install
}

proc cookit::thread::vfsfilelist-static {} {
    set filelist [list]
    set dir [file join [cookit::getInstallStaticDirectory] lib thread[cookit::getPartVersion thread]]
    set libdir "lib/thread[cookit::getPartVersion thread]"
    set rc [cookit::filterFilelist \
        [cookit::listAllFiles $libdir $dir] \
	include match *.tcl \
        exclude match */template/* \
        exclude match */thread.tcl \
        ]
}

proc cookit::thread::linkerflags-static {} {
    return [list]
}

proc cookit::thread::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf
    set rc [list]
    
    # TODO: rewrite!
    set filelist [glob -nocomplain -directory [cookit::getBuildStaticDirectory thread] \
            "{thread,libthread}*[cookit::osSpecific .a]"]
    
    if {[llength $filelist] != 1} {
        error "Unable to identify VFS .a file - [list $filelist]"
    }
    
    lappend rc [cookit::wdrelative [lindex $filelist 0]]
    
    return $rc
}

proc cookit::thread::initialize-dynamic {version} {
}

proc cookit::thread::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative -mode dynamic
}

proc cookit::thread::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::thread::install-dynamic {} {
    # this is not needed
    cookit::buildMake install
}

proc cookit::thread::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tlsdir ""
    foreach g [glob -directory $basedir lib/thread*] {
        set gt [file tail $g]
        if {[regexp -nocase "thread(-|)\[0-9\.]+\$" $gt]} {
            set threaddir $g
            break
        }
    }

    if {$threaddir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        set customfiles [list]

        lappend packages "thread-[string range $gt 6 end]" [concat $filelist $customfiles]
    }

    return $packages
}

