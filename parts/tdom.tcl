namespace eval cookit::tdom {}

cookit::partRegister tdom "tDOM"

proc cookit::tdom::retrievesource {} {
    set filename "tDOM-0.8.3.tgz"
    set fileurl "https://github.com/downloads/tDOM/tdom/$filename"
    set destfile [file join [cookit::getDownloadsDirectory] \
        [string map [list .tgz .tar.gz tDOM tdom] $filename]]
    cookit::downloadURL $fileurl $destfile
}

proc cookit::tdom::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tdom::initialize-dynamic {version} {
    set fn [file join [cookit::getSourceDirectory tdom] generic/tcldom.c]
    set fh [open $fn r]
    fconfigure $fh -translation binary
    set sfc [read $fh]
    close $fh
    set dfc [string map [list "interp->errorLine" "-1"] $sfc]
    if {![string equal $sfc $dfc]} {
	set fh [open $fn w]
	fconfigure $fh -translation binary
	puts -nonewline $fh $dfc
	close $fh
    }
}

proc cookit::tdom::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::tdom::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtdom*.so tdom*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tdom::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tdom::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tdom::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tdomdir ""
    foreach g [glob -directory $basedir lib/tdom*] {
        set gt [file tail $g]
        if {[regexp -nocase "tdom(-|)\[0-9\.]+\$" $gt]} {
            set tdomdir $g
            break
        }
    }

    if {$tdomdir != ""} {
	set ver [string range $gt 4 end]
	set fh [open [file join $tdomdir pkgIndex.tcl] r]
	fconfigure $fh -translation binary
	set fc [read $fh]
	close $fh

	set dfc [string map [list "$ver\"load" "$ver \"load"] $fc]
	if {![string equal $fc $dfc]} {
	    set fh [open [file join $tdomdir pkgIndex.tcl] w]
	    fconfigure $fh -translation binary
	    puts -nonewline $fh $dfc
	    close $fh
	}


        lappend packages "tdom-$ver" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}
