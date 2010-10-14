namespace eval cookit::tclcompiler {}

cookit::partRegister tclcompiler "Tclcompiler"

proc cookit::tclcompiler::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tclpro.cvs.sourceforge.net:/cvsroot/tclpro tclcompiler]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # TODO: rework
    set version 8.4

    # copy source to actual location
    cookit::copySourceRepository $tempdir tclcompiler $version
}

proc cookit::tclcompiler::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tclcompiler::initialize-dynamic {version} {
}

proc cookit::tclcompiler::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tclcompiler::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtclcompiler*.so tclcompiler*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tclcompiler::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tclcompiler::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tclcompiler::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tclcompilerdir ""
    foreach g [glob -directory $basedir lib/tclcompiler*] {
        set gt [file tail $g]
        if {[regexp -nocase "tclcompiler(-|)\[0-9\.]+\$" $gt]} {
            set tclcompilerdir $g
            break
        }
    }

    if {$tclcompilerdir != ""} {
        lappend packages "tclcompiler-[string range $gt 4 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}

