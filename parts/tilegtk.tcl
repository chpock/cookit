namespace eval cookit::tilegtk {}

cookit::partRegister tilegtk "Tile Gtk extension"

proc cookit::tilegtk::retrievesource {} {
    #set tempdir [cookit::gitExport git://tktable.git.sourceforge.net/gitroot/tktable/tile-gtk]
    set tempdir [cookit::gitExport git://github.com/aweelka/uTileGTK.git]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break

    # copy source to actual location
    cookit::copySourceRepository $tempdir tilegtk $version
}

proc cookit::tilegtk::parameters {version} {
    return [list \
        provides {} depends {tcl "" tk ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tilegtk::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tilegtk] tclconfig install-sh] -permissions 0755}
}

proc cookit::tilegtk::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative -with-tk relative \
        -mode dynamic
}

proc cookit::tilegtk::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtilegtk*.so tilegtk*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tilegtk::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tilegtk::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tilegtk::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tilegtkdir ""
    foreach g [glob -directory $basedir lib/tilegtk*] {
        set gt [file tail $g]
        if {[regexp -nocase "tilegtk(-|)(\[0-9\.]+)\$" $gt - - version]} {
            set tilegtkdir $g
            break
        }
    }

    if {$tilegtkdir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        set customfiles [list]

        lappend packages "tilegtk-$version" [concat $filelist $customfiles]
    }

    return $packages
}
