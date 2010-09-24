namespace eval cookit::tcludp {}

cookit::partRegister tcludp "Tcl UDP extension"

proc cookit::tcludp::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tcludp.cvs.sourceforge.net:/cvsroot/tcludp tcludp]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir tcludp $version
}

proc cookit::tcludp::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tcludp::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tcludp] tclconfig install-sh] -permissions 0755}
}

proc cookit::tcludp::configure-dynamic {} {
    set additional [list]

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic
}

proc cookit::tcludp::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtcludp1*.so tcludp1*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tcludp::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tcludp::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tcludp::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tcludpdir ""
    foreach g [glob -directory $basedir lib/udp1*] {
        set gt [file tail $g]
        if {[regexp -nocase "udp(-|)\[0-9\.]+\$" $gt]} {
            set tcludpdir $g
            break
        }
    }

    if {$tcludpdir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        set customfiles [list]

        lappend packages "tcludp-[string range $gt 3 end]" [concat $filelist $customfiles]
    }

    return $packages
}
