namespace eval cookit::tktreectrl {}

cookit::partRegister tktreectrl "Tktreectrl"

proc cookit::tktreectrl::retrievesource {} {
    set fileurl "http://sourceforge.net/projects/tktreectrl/files/tktreectrl/tktreectrl-2.3.2/tktreectrl-2.3.2.tar.gz/download"
    set filename "tktreectrl-2.3.2.tar.gz"
    set destfile [file join [cookit::getDownloadsDirectory] $filename]

    cookit::downloadURL $fileurl $destfile 
}

proc cookit::tktreectrl::parameters {version} {
    return [list \
        provides {} depends {tcl "" tk ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tktreectrl::initialize-dynamic {version} {
}

proc cookit::tktreectrl::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative -with-tk relative \
        -mode dynamic

}

proc cookit::tktreectrl::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtktreectrl*.so tktreectrl*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tktreectrl::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tktreectrl::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tktreectrl::packageslist-dynamic {} {
    set ver [cookit::getPartVersion tktreectrl]
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tktreectrldir ""
    foreach g [glob -directory $basedir lib/treectrl*] {
        set gt [file tail $g]
        if {[regexp -nocase "treectrl(-|)\[0-9\.]+\$" $gt]} {
            set tktreectrldir $g
            break
        }
    }

    if {$tktreectrldir != ""} {
        lappend packages "tktreectrl-[string range $gt 8 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            exclude match lib/treectrl$ver/htmldoc/* \
            ]
    }

    return $packages
}

