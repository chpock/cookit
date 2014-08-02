namespace eval cookit::sqlite3 {}

cookit::partRegister sqlite3 "SQLite3"

proc cookit::sqlite3::retrievesource {} {
    set url "http://www.sqlite.org/download.html"
    set html [cookit::downloadURL $url]
    if {![regexp {'([0-9]+/)(sqlite-autoconf[^\.]+\.tar.gz)'} $html - prefix filename]} {
        error "Unable to download sqlite3 sources - no URL on website"
    }
    set prefix [string trim $prefix /]
    set fileurl "http://www.sqlite.org/$prefix/$filename"
    cookit::log 5 "Downloading from $fileurl"

    set tempdir [cookit::getTempDirectory]
    set tempfile [file join $tempdir $filename]

    cookit::downloadURL $fileurl $tempfile
    set pwd [pwd]
    cd $tempdir
    set tempdir [pwd]
    if {[catch {
        exec tar -xzf $filename
        file delete -force $filename
    } err]} {
        cookit::log 2 "Extracting failed: $err"
        cd $pwd
        error "Extracting sqlite3 failed"
    }
    cd $pwd
    set g [glob -directory $tempdir *]
    if {([llength $g] == 1) && ([file type $g] == "directory")} {
        set basedir $g
    }  else  {
        set basedir $tempdir
    }

    set fh [open [file join $basedir configure] r]
    set fc [read $fh]
    close $fh

    # figure out actual version
    if {![regexp -line "^\\s*PACKAGE_VERSION=(.*)\\s*\$" $fc - version]} {
        error "Unknown sqlite3 version"
    }
    set version [string trim $version "\"' \t"]

    # copy source to actual location
    cookit::copySourceRepository $basedir sqlite3 $version
}

proc cookit::sqlite3::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::sqlite3::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory sqlite3] tea tclconfig install-sh] -permissions 0755}
    catch {file attributes [file join [cookit::getSourceDirectory sqlite3] tclconfig install-sh] -permissions 0755}
}

proc cookit::sqlite3::configure-dynamic {} {
    if {[file exists [file join [cookit::getSourceDirectory sqlite3] tea]]} {
        set subdirectory "tea"
    }  else  {
        set subdirectory ""
    }
    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic -subdirectory $subdirectory

    # change Makefile on OSX to disable NFS filesystem fix for OSX as it causes issues on <=10.4
    # and enable -DSQLITE_WITHOUT_ZONEMALLOC after http://sqlite.1065341.n5.nabble.com/PATCH-Fix-quot-Symbol-not-found-OSAtomicCompareAndSwapPtrBarrier-quot-on-Mac-OS-X-10-4-Tiger-td63847.html
    if {[string match "macosx-*" $::cookit::platform]} {
        set fn [file join [cookit::getBuildDynamicDirectory sqlite3] Makefile]
        set fh [open $fn r]
        fconfigure $fh -translation binary
        set fc [read $fh]
        close $fh

        regsub -all -line -- "^(CFLAGS\\s+=.*)" $fc "\\1 -DSQLITE_ENABLE_LOCKING_STYLE=0 -DSQLITE_WITHOUT_ZONEMALLOC" fc

        set fh [open $fn w]
        fconfigure $fh -translation binary
        puts -nonewline $fh $fc
        close $fh
    }
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
        if {[regexp -nocase "sqlite(3\[0-9\\.]+)\$" $gt - version]} {
            set sqlite3dir $g
            break
        }
    }

    if {$sqlite3dir != ""} {
        lappend packages "sqlite3-$version" [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
    }

    return $packages
}
