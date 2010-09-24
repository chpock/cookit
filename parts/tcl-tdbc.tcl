namespace eval cookit::tcl-tdbc {}

proc cookit::tcl-tdbc::getSourceDirectory {version} {
    set srcdir [file join [cookit::getSourceDirectory tcl] pkgs tdbc]
    return $srcdir
}

proc cookit::tcl-tdbc:::getVersions {} {
    set dir [getSourceDirectory ""]
    if {![file exists $dir]} {
	return [list]
    }
    set fh [open [file join $dir configure.in] r]
    set fc [read $fh]
    close $fh
    #AC_INIT([tdbc], [4.0b3])
    if {![regexp -line "AC_INIT\\(.*?,(.*)\\)" $fc - version]} {
        error "Unable to get version information for TDBC"
    }
    set version [string trim $version " ,\[\]\t"]
    return [list $version]
}

cookit::partRegister tcl-tdbc "TDBC in Tcl 8.6"

proc cookit::tcl-tdbc::retrievesource {} {
}

proc cookit::tcl-tdbc::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tcl-tdbc::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tcl-tdbc] tclconfig install-sh] -permissions 0755}
}

proc cookit::tcl-tdbc::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tcl-tdbc::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtdbc*.so tdbc*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tcl-tdbc::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tcl-tdbc::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tcl-tdbc::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tdbcdir ""
    foreach g [glob -directory $basedir lib/tdbc*] {
        set gt [file tail $g]
        if {[regexp -nocase "tdbc(-|)\[0-9\\.abcd\]+\$" $gt]} {
            set tdbcdir $g
            break
        }
    }

    if {$tdbcdir != ""} {
        lappend packages "tdbc-[string range $gt 4 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            exclude match *.a \
            exclude match *.sh \
            ]
    }

    return $packages
}

