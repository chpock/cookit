namespace eval cookit::tk {}

cookit::partRegister tk "Tk GUI"
set cookit::allOptions(tk-xft) {{Enables XFT support}}
set cookit::allOptions(tk-aqua.arg) {{yes} {Value for --enable-aqua parameter}}

proc cookit::tk::retrievesource {} {
    if {[info exists ::cookit::opt(tk)] && [string match 8.5.* $::cookit::opt(tk)]} {
	set r "core-8-5-branch"
    }  else  {
	set r "trunk"
    }
    set tempdir [cookit::fossilExport http://core.tcl.tk/tk $r]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # figure out actual version
    cookit::regexpFileLine [file join $tempdir generic tk.h] \
        "#define\\s+TK_PATCH_LEVEL\\s+\"(.*?)\"" - version

    # copy source to actual location
    cookit::copySourceRepository $tempdir tk $version
}

proc cookit::tk::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        configfile tkConfig.sh \
        buildmodes {static dynamic} \
        ]
}

proc cookit::tk::initialize-static {version} {
    set dir1 [file join [cookit::getSourceDirectory tk] $::cookit::unixwinplatform]
    set dir2 [file join [cookit::getSourceDirectory tk] generic]
    set dir3 [file join [cookit::getInstallStaticDirectory] include]

    cookit::addParameters cookitIncludeCode "#include <tk.h>\n"
    cookit::addParameters cookitExternCode "Tcl_AppInitProc Tk_Init, Tk_SafeInit;\n"
    cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"Tk\", Tk_Init, Tk_SafeInit);\n"
    cookit::addParametersList cookitCompileFlags -I$dir1 -I$dir2 -I$dir3
    cookit::addParametersList cookitCompileFlags -DKIT_INCLUDES_TK
}

proc cookit::tk::configure-static {} {
    set additional [list]
    if {$::cookit::platform != "win32-x86"} {
        if {$::cookit::opt(tk-xft)} {
            lappend additional --enable-xft
        }  else  {
            lappend additional --disable-xft
        }
    }

    if {[string match "macosx-*" $::cookit::platform]} {
	lappend additional --enable-aqua=$::cookit::opt(tk-aqua)
    }

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -platform -mode static -additional $additional
}

proc cookit::tk::build-static {} {
    set fn [file join [cookit::getBuildStaticDirectory tk] Makefile]
    set fh [open $fn r]
    fconfigure $fh -translation binary
    set fc [read $fh]
    close $fh

    regsub -all -line -- "-DUSE_TCL_STUBS" $fc "" fc

    set fh [open $fn w]
    fconfigure $fh -translation binary
    puts -nonewline $fh $fc
    close $fh

    if {$::cookit::platform == "win32-x86"} {
        # on Windows, wish does not build sometimes when doing static builds
        cookit::loadConfigFile tcl conf
        cookit::loadConfigFile tk conf

        cookit::buildMake libraries
        cookit::buildMake $conf(TK_LIB_FILE)
    }  else  {
        cookit::buildMake all
    }
}

proc cookit::tk::install-static {} {
    if {$::cookit::platform == "win32-x86"} {
        cookit::buildMake install-libraries
    }  else  {
        cookit::buildMake install
    }
}

proc cookit::tk::vfsfilelist-static {} {
    set filelist [list]
    set dir1 [cookit::getInstallStaticDirectory]
    set version [cookit::getPartVersion tk]
    set ver [string range $version 0 2]

    set filelist1 [cookit::filterFilelist \
        [cookit::listAllFiles lib/tk$ver [file join $dir1 lib tk$ver]] \
        exclude match lib/tk$ver/tkAppInit.c \
        exclude match lib/tk$ver/demos \
        exclude match lib/tk$ver/demos/* \
        exclude match lib/tk$ver/images \
        exclude match lib/tk$ver/images/* \
        exclude match lib/tk$ver/*.a \
        exclude match lib/tk$ver/*.o \
        exclude match lib/tk$ver/*.obj \
        exclude match lib/tk$ver/*.lib \
        ]

    lappend filelist lib/tk$ver/pkgIndex.tcl data "package ifneeded Tk [list $version] \{load \{\} Tk\}\n" ""

    return [concat $filelist $filelist1]
}

proc cookit::tk::linkerflags-static {} {

    set rc [list]

    cookit::loadConfigFile tcl conf
    cookit::loadConfigFile tk conf

    set rc [concat $rc [lrange $conf(TK_LIBS) 0 end]]

    if {$::cookit::platform == "win32-x86"} {
        lappend rc -mwindows
    }

    return $rc
}

proc cookit::tk::linkerfiles-static {} {
    set rc [list]
    
    cookit::loadConfigFile tcl conf
    cookit::loadConfigFile tk conf

    lappend rc [cookit::wdrelative [file join [cookit::getBuildStaticDirectory tk] $conf(TK_LIB_FILE)]]

    return $rc
}

proc cookit::tk::initialize-dynamic {version} {
    catch {file attributes [file join [cookit::getSourceDirectory tk] unix install-sh] -permissions 0755}
    catch {file copy -force [file join [cookit::getSourceDirectory tk] unix install-sh] [file join [cookit::getSourceDirectory tk] macosx install-sh]}
    catch {file attributes [file join [cookit::getSourceDirectory tk] macosx install-sh] -permissions 0755}
}

proc cookit::tk::configure-dynamic {} {
    set additional [list]
    if {$::cookit::platform != "win32-x86"} {
        if {$::cookit::opt(tk-xft)} {
            lappend additional --enable-xft
        }  else  {
            lappend additional --disable-xft
        }
    }

    if {[string match "macosx-*" $::cookit::platform]} {
	lappend additional --enable-aqua=$::cookit::opt(tk-aqua)
    }

    cookit::buildConfigure -sourcepath relative -with-tcl relative \
        -platform -mode dynamic -additional $additional
}

proc cookit::tk::build-dynamic {} {
    set fn [file join [cookit::getBuildDynamicDirectory tk] Makefile]
    if {$::cookit::platform == "win32-x86"} {
        # on Windows, wish does not build sometimes when doing static builds
        cookit::loadConfigFile tcl conf
        cookit::loadConfigFile tk conf

        cookit::buildMake libraries
        cookit::buildMake $conf(TK_LIB_FILE)
    }  else  {
        cookit::buildMake all
    }
}

proc cookit::tk::install-dynamic {} {
    if {$::cookit::platform == "win32-x86"} {
        cookit::buildMake install
    }  else  {
        cookit::buildMake install
    }
}

proc cookit::tk::vfsfilelist-dynamic {} {
    set filelist [list]
    set dir1 [cookit::getInstallDynamicDirectory]
    set ver 8.6

    # TODO: needs fixing    

    set filelist1 [cookit::filterFilelist \
        [cookit::listAllFiles lib/tk$ver [file join $dir1 lib tk$ver]] \
        exclude match lib/tk$ver/tkAppInit.c \
        exclude match lib/tk$ver/demos \
        exclude match lib/tk$ver/demos/* \
        exclude match lib/tk$ver/*.a \
        exclude match lib/tk$ver/*.o \
        exclude match lib/tk$ver/*.obj \
        exclude match lib/tk$ver/*.lib \
        ]

    return [concat $filelist $filelist1]
}

proc cookit::tk::packageslist-dynamic {} {
    set packages [list]
    
    # we do not create package for Tk on Windows
    if {$::cookit::platform != "win32-x86"} {
	set basedir [cookit::getInstallDynamicDirectory]

	set version [cookit::getPartVersion tk]
	set ver [cookit::buildVersionString $version 2]
	set dver [string map [list "." ""] $ver]
	set dirname "tk$ver"

	set tkpath [file join $basedir lib $dirname]

	if {[file exists $tkpath]} {
	    set tkparentdir [file dirname $tkpath]
	    set libfiles {}
	    foreach libfile [glob \
		[file join $tkparentdir libtk$ver*[info sharedlibextension]] \
		[file join $tkparentdir tk$ver*[info sharedlibextension]] \
		[file join $tkparentdir libtk$dver*[info sharedlibextension]] \
		[file join $tkparentdir tk$dver*[info sharedlibextension]] \
	    ] {
		lappend libfiles lib/[file tail $libfile] file $libfile ""
	    }

	    lappend packages "tk-$version" [concat \
		[cookit::filterFilelist \
		    [cookit::listAllFiles "lib/tk$ver" $tkpath] \
		    exclude match lib/tk$ver/tkAppInit.c \
		    exclude match lib/tk$ver/demos \
		    exclude match lib/tk$ver/demos/* \
		    exclude match lib/tk$ver/*.a \
		    exclude match lib/tk$ver/*.o \
		    exclude match lib/tk$ver/*.obj \
		    exclude match lib/tk$ver/*.lib \
		    ] \
		$libfiles \
		]
	}
    }

    return $packages
}
