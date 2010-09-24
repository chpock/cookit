namespace eval cookit::tcl-itcl {}

proc cookit::tcl-itcl::getSourceDirectory {version} {
    set srcdir [file join [cookit::getSourceDirectory tcl] pkgs itcl]
    return $srcdir
}

proc cookit::tcl-itcl:::getVersions {} {
    set dir [getSourceDirectory ""]
    if {![file exists $dir]} {
	return [list]
    }
    set fh [open [file join $dir configure.in] r]
    set fc [read $fh]
    close $fh
    #AC_INIT([itcl], [4.0b3])
    if {![regexp -line "AC_INIT\\(.*?,(.*)\\)" $fc - version]} {
        error "Unable to get version information for Itcl"
    }
    set version [string trim $version " ,\[\]\t"]
    return [list $version]
}

cookit::partRegister tcl-itcl "IncrTcl in Tcl 8.6"

proc cookit::tcl-itcl::retrievesource {} {
}

proc cookit::tcl-itcl::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tcl-itcl::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tcl-itcl] tclconfig install-sh] -permissions 0755}
}

proc cookit::tcl-itcl::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tcl-itcl::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libitcl*.so itcl*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tcl-itcl::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tcl-itcl::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tcl-itcl::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set itcldir ""
    foreach g [glob -directory $basedir lib/itcl*] {
        set gt [file tail $g]
        if {[regexp -nocase "itcl(-|)\[0-9\\.abcd\]+\$" $gt]} {
            set itcldir $g
            break
        }
    }

    if {$itcldir != ""} {
        lappend packages "itcl-[string range $gt 4 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            exclude match *.a \
            exclude match *.sh \
            ]
    }

    return $packages
}

