namespace eval cookit::sqlite3 {}

cookit::partRegister sqlite3 "SQLite3"

proc cookit::sqlite3::retrievesource {} {
    set url "http://www.sqlite.org/download.html"
    set html [cookit::downloadURL $url]
    if {![regexp "<a href=\"(sqlite-\[A-Za-z0-9\\._\\-\]+-tea.tar.gz)\">" $html - filename]} {
        error "Unable to download sqlite3 sources - no URL on website"
    }
    set fileurl "http://www.sqlite.org/$filename"
    set destfile [file join [cookit::getDownloadsDirectory] \
        [string map [list _ . -tea "" "sqlite-" "sqlite3-"] $filename]]

    cookit::downloadURL $fileurl $destfile
}

proc cookit::sqlite3::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::sqlite3::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory sqlite3] tclconfig install-sh] -permissions 0755}
}

proc cookit::sqlite3::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic

}

proc cookit::sqlite3::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libsqlite3*.so sqlite3*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::sqlite3::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::sqlite3::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::sqlite3::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set sqlite3dir ""
    foreach g [glob -directory $basedir lib/sqlite3*] {
        set gt [file tail $g]
        if {[regexp -nocase "sqlite3(-|)\[0-9\.]+\$" $gt]} {
            set sqlite3dir $g
            break
        }
    }

    if {$sqlite3dir != ""} {
        lappend packages "sqlite3-3.[string range $gt 8 end]" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}
