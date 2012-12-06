namespace eval cookit::cookfs {}

cookit::partRegister cookfs "CooKit VFS"

set cookit::allOptions(cookfs-bz2) {{Enables bzip2 support}}
set cookit::allOptions(cookfs-tcl-fallback) {{Enables tcl fallback support}}
set cookit::allOptions(cookfs-internal-debug) {{Enables internal debug}}

proc cookit::cookfs::retrievesource {} {
    if {![catch {
	set tempdir [cookit::svnExport svn://svn.code.sf.net/p/cookit/code/cookfs]
    }]} {
	# get base version from configure.in
	foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
	
	# copy source to actual location
	cookit::copySourceRepository $tempdir cookfs $version
    }  else  {
	# download over http
	# TODO: remove hardcoded URL
	set url "http://sourceforge.net/projects/cookit/files/cookfs/1.1/cookfs-1.1-sources.tar.gz/download"
	set filename "cookfs-1.1-sources.tar.gz"
	set destfile [file join [cookit::getDownloadsDirectory] $filename]
	cookit::downloadURL $url $destfile
    }
}

proc cookit::cookfs::parameters {version} {
    return [list \
        retrieveByDefault 1 \
        provides {} depends {tcl ""} \
	buildmodes {static dynamic} \
        ]
}

proc cookit::cookfs::initialize-static {version} {
}

proc cookit::cookfs::configure-static {} {
    set additional [list]
    if {$::cookit::opt(cookfs-bz2)} {
	lappend additional --enable-bz2
    }
    if {$::cookit::opt(cookfs-tcl-fallback)} {
	lappend additional --enable-tcl-fallback
    }
    if {$::cookit::opt(cookfs-internal-debug)} {
	lappend additional --enable-internal-debug
    }

    cookit::buildConfigure -sourcepath relative -with-tcl relative -mode static -additional $additional
}

proc cookit::cookfs::build-static {} {
    cookit::buildMake all
}

proc cookit::cookfs::install-static {} {
    # this is not needed
    #cookit::buildMake install
}

proc cookit::cookfs::vfsfilelist-static {} {
    set filelist [list]
}

proc cookit::cookfs::linkerflags-static {} {
    return [list]
}

proc cookit::cookfs::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf
    set rc [list]
    
    # TODO: rewrite!
    set filelist [glob -nocomplain -directory [cookit::getBuildStaticDirectory cookfs] \
            "{cookfs,libcookfs}*[cookit::osSpecific .a]"]
    
    if {[llength $filelist] != 1} {
        error "Unable to identify cookfs .a file"
    }
    
    lappend rc [cookit::wdrelative [lindex $filelist 0]]
    
    return $rc
}


proc cookit::cookfs::initialize-dynamic {version} {
}

proc cookit::cookfs::configure-dynamic {} {
    set additional [list]
    if {$::cookit::opt(cookfs-bz2)} {
	lappend additional --enable-bz2
    }
    if {$::cookit::opt(cookfs-tcl-fallback)} {
	lappend additional --enable-tcl-fallback
    }

    cookit::buildConfigure -sourcepath relative -with-tcl relative -mode dynamic -additional $additional
}

proc cookit::cookfs::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::cookfs::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::cookfs::packageslist-dynamic {} {
    set packages [list]
    
    set basedir [cookit::getInstallDynamicDirectory]
    set cookfsdir ""
    foreach g [glob -directory $basedir lib/cookfs*] {
        set gt [file tail $g]
        if {[regexp -nocase "cookfs(-|)\[0-9\.]+\$" $gt]} {
            set cookfsdir $g
            break
        }
    }

    if {$cookfsdir != ""} {
        set customfiles {}
        set filelist [cookit::filterFilelist \
            [cookit::listAllFiles "lib/$gt" $g] \
            ]
        lappend packages "cookfs-[string range $gt 6 end]" [concat $filelist $customfiles]
    }

    return $packages
}

