namespace eval cookit::tclx {}

cookit::partRegister tclx "TclX"

proc cookit::tclx::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tclx.cvs.sourceforge.net:/cvsroot/tclx tclx]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # TODO: rework
    set version 8.4

    # copy source to actual location
    cookit::copySourceRepository $tempdir tclx $version
}

proc cookit::tclx::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tclx::initialize-dynamic {version} {
}

proc cookit::tclx::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tclx::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtclx*.so tclx*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tclx::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tclx::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tclx::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tclxdir ""
    foreach g [glob -directory $basedir lib/tclx*] {
        set gt [file tail $g]
        if {[regexp -nocase "tclx(-|)\[0-9\.]+\$" $gt]} {
            set tclxdir $g
            break
        }
    }

    if {$tclxdir != ""} {
        lappend packages "tclx-[string range $gt 4 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}

