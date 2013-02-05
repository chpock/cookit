namespace eval cookit::memchan {}

cookit::partRegister memchan "Tcl memchan extension"

proc cookit::memchan::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@memchan.cvs.sourceforge.net:/cvsroot/memchan memchan]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir memchan $version
}

proc cookit::memchan::parameters {version} {
    return [list \
        provides {virtual:memchan 1.0} depends {tcl ""} \
	buildmodes {static} \
        ]
}

proc cookit::memchan::initialize-static {version} {
    cookit::addParameters cookitExternCode "Tcl_AppInitProc Memchan_Init;\n" memchan-1
    cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"Memchan\", Memchan_Init, NULL);\n" memchan-2
}

proc cookit::memchan::configure-static {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative
}

proc cookit::memchan::build-static {} {
    cookit::buildMake binaries
    cookit::buildMake libraries
}

proc cookit::memchan::install-static {} {
    # this is not needed
}

proc cookit::memchan::vfsfilelist-static {} {
    set rc {}
    set version [cookit::getPartVersion memchan]
    set pkgindex "package ifneeded Memchan [list $version] {load {} Memchan}\n"
    lappend rc lib/Memchan$version/pkgIndex.tcl data $pkgindex ""
    return $rc
}

proc cookit::memchan::linkerflags-static {} {
    return [list]
}

proc cookit::memchan::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf
    set rc [list]
    
    # TODO: rewrite!
    set filelist {}
    
    # this is needed to skip *stub* and properly find *.a file
    foreach f [glob -nocomplain -directory [cookit::getBuildStaticDirectory memchan] \
            "{memchan,libmemchan,Memchan,libMemchan}*[cookit::osSpecific .a]"] {
	if {![string match -nocase "*memchanstub*" $f]} {
	    if {[lsearch -exact $filelist $f] < 0} {
		lappend filelist $f
	    }
	}
    }

    if {[llength $filelist] != 1} {
        error "Unable to identify Memchan .a file - [list $filelist]"
    }
    
    lappend rc [cookit::wdrelative [lindex $filelist 0]]
    
    return $rc
}

