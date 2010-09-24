namespace eval cookit::tileqt {}

cookit::partRegister tileqt "Tile Qt extension"

proc cookit::tileqt::retrievesource {} {
    #set tempdir [cookit::gitExport git://tktable.git.sourceforge.net/gitroot/tktable/tile-qt]
    set tempdir [cookit::gitExport git://github.com/aweelka/uTileQt.git]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break

    # copy source to actual location
    cookit::copySourceRepository $tempdir tileqt $version
}

proc cookit::tileqt::parameters {version} {
    return [list \
        provides {} depends {tcl "" tk ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tileqt::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tileqt] tclconfig install-sh] -permissions 0755}
}

proc cookit::tileqt::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative -with-tk relative \
        -mode dynamic
}

proc cookit::tileqt::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtileqt*.so tileqt*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tileqt::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tileqt::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tileqt::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tileqtdir ""
    foreach g [glob -directory $basedir lib/tileqt*] {
        set gt [file tail $g]
        if {[regexp -nocase "tileqt(-|)(\[0-9\.]+)\$" $gt - - version]} {
            set tileqtdir $g
            break
        }
    }

    if {$tileqtdir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        set customfiles [list]

        lappend packages "tileqt-$version" [concat $filelist $customfiles]
    }

    return $packages
}
