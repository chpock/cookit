namespace eval cookit::vfs {}

cookit::partRegister vfs "Tcl VFS handling"

proc cookit::vfs::retrievesource {} {
    set tempdir [cookit::cvsExport :pserver:anonymous@tclvfs.cvs.sourceforge.net:/cvsroot/tclvfs tclvfs]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # copy source to actual location
    cookit::copySourceRepository $tempdir vfs $version
}

proc cookit::vfs::parameters {version} {
    return [list \
        retrieveByDefault 1 \
        provides {} depends {tcl ""} \
	buildmodes {static dynamic} \
        ]
}

proc cookit::vfs::initialize-static {version} {
}

proc cookit::vfs::configure-static {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative
}

proc cookit::vfs::build-static {} {
    cookit::buildMake all
}

proc cookit::vfs::install-static {} {
    # this is not needed
    cookit::buildMake install
}

proc cookit::vfs::vfsfilelist-static {} {
    set filelist [list]
    set dir [file join [cookit::getInstallStaticDirectory] lib vfs[cookit::getPartVersion vfs]]
    set libdir "lib/vfs[cookit::getPartVersion vfs]"
    set rc [cookit::filterFilelist \
        [cookit::listAllFiles $libdir $dir] \
	include match *.tcl \
        exclude match */template/* \
        exclude match */vfs.tcl \
        ]

    set fh [open [file join $dir vfs.tcl] r]
    fconfigure $fh -translation binary
    set vfstcl [read $fh]
    close $fh

    regsub -all -line -- "^\\s+load\\s+.*vfs.*\$" $vfstcl {load {} vfs} vfstcl

    lappend rc "$libdir/vfs.tcl" data $vfstcl ""

    return $rc
}

proc cookit::vfs::linkerflags-static {} {
    return [list]
}

proc cookit::vfs::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf
    set rc [list]
    
    # TODO: rewrite!
    set filelist [glob -nocomplain -directory [cookit::getBuildStaticDirectory vfs] \
            "{vfs,libvfs}*[cookit::osSpecific .a]"]
    
    if {[llength $filelist] != 1} {
        error "Unable to identify VFS .a file - [list $filelist]"
    }
    
    lappend rc [cookit::wdrelative [lindex $filelist 0]]
    
    return $rc
}

proc cookit::vfs::initialize-dynamic {version} {
}

proc cookit::vfs::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative -with-tcl relative -mode dynamic
}

proc cookit::vfs::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::vfs::install-dynamic {} {
    # this is not needed
    cookit::buildMake install
}

proc cookit::vfs::packageslist-dynamic {} {
    return [list]
}

