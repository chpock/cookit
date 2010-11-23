namespace eval cookit::tls {}

cookit::partRegister tls "TLS: OpenSSL Tcl Extension"

proc cookit::tls::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tls.cvs.sourceforge.net:/cvsroot/tls tls]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir tls $version
}

proc cookit::tls::parameters {version} {
    return [list \
        provides {} depends {openssl "" tcl ""} \
        buildmodes {static dynamic} \
        ]
}

proc cookit::tls::initialize-static {version} {
    cookit::addParameters cookitExternCode "Tcl_AppInitProc Tls_Init;\n" tls-1
    cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"Tls\", Tls_Init, NULL);\n" tls-2
}

proc cookit::tls::configure-static {} {
    set additional [list]

    lappend additional --with-ssl-dir=[file join [cookit::getInstallStaticDirectory] openssl]

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode static -additional $additional

    set fh [open Makefile r]
    fconfigure $fh -translation binary
    set makefilefc [read $fh]
    close $fh

    regsub -- {-lssleay32 -llibeay32} $makefilefc {-lssl -lcrypto -lws2_32 -lgdi32} makefilefc

    set fh [open Makefile w]
    fconfigure $fh -translation binary
    puts -nonewline $fh $makefilefc
    close $fh
}

proc cookit::tls::build-static {} {
    cookit::buildMake all
}

proc cookit::tls::install-static {} {
    cookit::buildMake install
}

proc cookit::tls::vfsfilelist-static {} {
    set filelist [list]

    set dir1 [cookit::getInstallStaticDirectory]
    set version [cookit::getPartVersion tls]
    if {$version == "1.6.0"} {
	set ver "1.6"
    }  else  {
	set ver $version
    }

    set pkgindex "package ifneeded tls $ver \"\[list source \[file join \$dir tls.tcl\]\] ; load {} Tls ; rename tls::initlib \{\}\"\n"
    lappend filelist lib/tls$ver/pkgIndex.tcl data $pkgindex ""
    lappend filelist lib/tls$ver/tls.tcl file [file join $dir1 lib tls$ver tls.tcl] ""

    return $filelist
}

proc cookit::tls::linkerflags-static {} {
    return [list]
}

proc cookit::tls::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf

    set rc [list]
    set g [glob -directory [cookit::getBuildStaticDirectory tls] *tls*.a]
    if {[llength $g] != 1} {error "Unable to find tls static package."}
    lappend rc [cookit::wdrelative [lindex $g 0]]
    return $rc
}

proc cookit::tls::initialize-dynamic {version} {
}

proc cookit::tls::configure-dynamic {} {
    set additional [list]

    lappend additional --with-ssl-dir=[file join [cookit::getInstallDynamicDirectory] openssl]

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -mode dynamic -additional $additional

    if {$::cookit::platform == "win32-x86"} {
        set fh [open Makefile r]
        fconfigure $fh -translation binary
        set makefilefc [read $fh]
        close $fh

        regsub -- {-lssleay32 -llibeay32} $makefilefc {-lssl -lcrypto -lws2_32 -lgdi32} makefilefc

        set fh [open Makefile w]
        fconfigure $fh -translation binary
        puts -nonewline $fh $makefilefc
        close $fh
    }
}

proc cookit::tls::build-dynamic {} {
    cookit::buildMake all

    set libfiles [glob -nocomplain libtls1*.so tls1*.dll]

    if {[llength $libfiles] == 1} {
        set cmd [list strip [lindex $libfiles 0]]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }  else  {
        cookit::log 2 "cookit::tls::build-dynamic: Unable to find library to strip - result: [list $libfiles]"
    }

}

proc cookit::tls::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::tls::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set tlsdir ""
    foreach g [glob -directory $basedir lib/tls*] {
        set gt [file tail $g]
        if {[regexp -nocase "tls(-|)\[0-9\.]+\$" $gt]} {
            set tlsdir $g
            break
        }
    }

    if {$tlsdir != ""} {
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            exclude match */tls.tcl \
            ]
        set customfiles [list]

        set fh [open [file join $tlsdir tls.tcl] r]
        fconfigure $fh -translation auto -encoding iso8859-1
        set fc [read $fh]
        close $fh
        set newproc {proc tls::initlib {dir dll} {uplevel #0 [list load [file join $dir $dll]]}}
        regsub "proc\\s+?tls::initlib.*?\[\r\n\]\}" $fc "$newproc\n" fc

        lappend customfiles lib/$gt/tls.tcl data $fc ""

        lappend packages "tls-[string range $gt 3 end]" [concat $filelist $customfiles]
    }

    return $packages
}
