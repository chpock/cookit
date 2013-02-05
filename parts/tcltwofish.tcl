namespace eval cookit::tcltwofish {}

cookit::partRegister tcltwofish "Tcl Twofish encryption"

proc cookit::tcltwofish::retrievesource {} {
    set tempdir [cookit::gitExport git://git.code.sf.net/p/tcltwofish/code]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break

    # copy source to actual location
    cookit::copySourceRepository $tempdir tcltwofish $version
}

proc cookit::tcltwofish::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tcltwofish::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tcltwofish] tclconfig install-sh] -permissions 0755}
}

proc cookit::tcltwofish::configure-dynamic {} {
    set additional [list]

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic
}

proc cookit::tcltwofish::build-dynamic {} {
    cookit::buildMake all

    if {![string match "macosx-*" $::cookit::platform]} {
        set libfiles [lsort -unique [glob -nocomplain libtcltwofish*.so tcltwofish*.dll tcltwofish*[info sharedlibextension] libtcltwofish*[info sharedlibextension]]]

        if {[llength $libfiles] == 1} {
            set cmd [list strip [lindex $libfiles 0]]
            set eid [cookit::startExtCommand $cmd]
            cookit::waitForExtCommand $eid 0
        }  else  {
            cookit::log 2 "cookit::tcltwofish::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
        }
    }
}

proc cookit::tcltwofish::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tcltwofish::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tcltwofishdir ""
    foreach g [glob -directory $basedir lib/tcltwofish*] {
        set gt [file tail $g]
        if {[regexp -nocase "tcltwofish(-|)\[0-9\.]+\$" $gt]} {
            set tcltwofishdir $g
            break
        }
    }

    if {$tcltwofishdir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        set customfiles [list]

        lappend packages "tcltwofish-[string range $gt 10 end]" [concat $filelist $customfiles]
    }

    return $packages
}
