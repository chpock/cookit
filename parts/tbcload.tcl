namespace eval cookit::tbcload {}

cookit::partRegister tbcload "Tbcload"

proc cookit::tbcload::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tclpro.cvs.sourceforge.net:/cvsroot/tclpro tbcload]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # TODO: rework
    set version 8.4

    # copy source to actual location
    cookit::copySourceRepository $tempdir tbcload $version
}

proc cookit::tbcload::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tbcload::initialize-dynamic {version} {
}

proc cookit::tbcload::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tbcload::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtbcload*.so tbcload*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tbcload::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tbcload::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tbcload::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tbcloaddir ""
    foreach g [glob -directory $basedir lib/tbcload*] {
        set gt [file tail $g]
        if {[regexp -nocase "tbcload(-|)\[0-9\.]+\$" $gt]} {
            set tbcloaddir $g
            break
        }
    }

    if {$tbcloaddir != ""} {
        lappend packages "tbcload-[string range $gt 7 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}

