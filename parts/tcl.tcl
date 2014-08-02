namespace eval cookit::tcl {}

cookit::partRegister tcl "Tcl language"

proc cookit::tcl::retrievesource {} {
    if {[info exists ::cookit::opt(tcl)] && [string match 8.5.* $::cookit::opt(tcl)]} {
	set r "core-8-5-branch"
    }  else  {
	set r "trunk"
    }
    set tempdir [cookit::fossilExport http://core.tcl.tk/tcl $r]
    
    # get base version from configure.in
    foreach {pkgname version} [cookit::getConfigureVersion $tempdir] break
    
    # figure out actual version
    cookit::regexpFileLine [file join $tempdir generic tcl.h] \
        "#define\\s+TCL_PATCH_LEVEL\\s+\"(.*?)\"" - version

    # copy source to actual location
    cookit::copySourceRepository $tempdir tcl $version
}

proc cookit::tcl::parameters {version} {
    set provides {}
    if {[string match 8.6* $version]} {
        lappend provides zlib 1.3 memchan 8.6
    }

    return [list \
        retrieveByDefault 1 \
        configfile tclConfig.sh \
        provides $provides depends {} \
	buildmodes {static dynamic} \
        ]
}

proc cookit::tcl::initialize-static {version} {
    if {$::cookit::platform == "win32-x86"} {
        cookit::addParameters cookitExternCode "Tcl_AppInitProc Dde_Init, Registry_Init;\n" tcl-win32-1
        cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"dde\", Dde_Init, NULL);\n" tcl-win32-2
        cookit::addParameters cookitAppinitCode "Tcl_StaticPackage(0, \"registry\", Registry_Init, NULL);\n" tcl-win32-3
    }
}

proc cookit::tcl::configure-static {} {
    cookit::buildConfigure -platform -sourcepath relative
}

proc cookit::tcl::build-static {} {
    if {$::cookit::unixwinplatform == "unix"} {
	set fh [open Makefile r]
	fconfigure $fh -translation binary
	set fc [read $fh]
	close $fh

	regsub -line -all -- {-DTCL_LIBRARY=\\".*\\"} $fc {-DTCL_LIBRARY=\\"/\\"} fc
	regsub -line -all -- {-DTCL_PACKAGE_PATH="\\".*\\""} $fc {-DTCL_PACKAGE_PATH="\\"/\\""} fc

	set fh [open Makefile w]
	fconfigure $fh -translation binary
	puts -nonewline $fh $fc
	close $fh
    }

    cookit::buildMake binaries
    cookit::buildMake libraries
}

proc cookit::tcl::install-static {} {
    cookit::buildMake install-binaries
    cookit::buildMake install-libraries
}

proc cookit::tcl::vfsfilelist-static {} {
    set filelist [list]

    set dir1 [cookit::getInstallStaticDirectory]
    # this might fail for Tcl 8.10 :-)
    set ver [string range [cookit::getPartVersion tcl] 0 2]
    set filelistcustom [list]

    set filelist1 [cookit::filterFilelist \
        [cookit::listAllFiles lib/tcl8 [file join $dir1 lib tcl8]] \
        ]

    set tcllibdir [file join $dir1 lib tcl$ver]

    # replace init.tcl - remove lib adding
    set fh [open [file join $tcllibdir init.tcl] r]
    fconfigure $fh -translation auto -encoding iso8859-1
    set fc [read $fh]
    close $fh

    # remove adding of ../lib to path
    regsub -all \
        "set\\s+?Dir.*?nameofexecutable.*?\\\]\\\]\\\]\\s+lib\\\].*?if\\s+\\\{.*?\\\}\\s+?\\\{.*?\\\}" \
	$fc "" fc

    lappend filelistcustom lib/tcl$ver/init.tcl data $fc ""

    set filelist2 [cookit::filterFilelist \
        [cookit::listAllFiles lib/tcl$ver [file join $dir1 lib tcl$ver]] \
        exclude match lib/tcl$ver/msgs \
        exclude match lib/tcl$ver/msgs/* \
        exclude match lib/tcl$ver/encoding/big5.enc \
        exclude match lib/tcl$ver/encoding/cp932.enc \
        exclude match lib/tcl$ver/encoding/cp936.enc \
        exclude match lib/tcl$ver/encoding/cp949.enc \
        exclude match lib/tcl$ver/encoding/cp950.enc \
        exclude match lib/tcl$ver/encoding/euc-cn.enc \
        exclude match lib/tcl$ver/encoding/euc-jp.enc \
        exclude match lib/tcl$ver/encoding/euc-kr.enc \
        exclude match lib/tcl$ver/encoding/gb12345.enc \
        exclude match lib/tcl$ver/encoding/gb2312.enc \
        exclude match lib/tcl$ver/encoding/gb2312-raw.enc \
        exclude match lib/tcl$ver/encoding/jis0208.enc \
        exclude match lib/tcl$ver/encoding/jis0212.enc \
        exclude match lib/tcl$ver/encoding/ksc5601.enc \
        exclude match lib/tcl$ver/encoding/macJapan.enc \
        exclude match lib/tcl$ver/encoding/shiftjis.enc \
        exclude match lib/tcl$ver/tzdata \
        exclude match lib/tcl$ver/tzdata/* \
        exclude match lib/tcl$ver/init.tcl \
        exclude match *.c \
        exclude match *.sh \
        ]

    if {$::cookit::platform == "win32-x86"} {
        set dir [cookit::getSourceDirectory tcl]

        set fh [open [file join $dir win tclWinDde.c] r]
        set fc [read $fh]
        close $fh

        if {[regexp -line "define\\s+TCL_DDE_VERSION\\s+\"(.*?)\"" $fc - ddever]} {
        }  elseif {[regexp -line "Tcl_PkgProvide\\s*\\(.*?, .*?, \"(.*?)\"" $fc - ddever]} {
        }  else  {
            error "Unable to determine DDE version"
        }

        set fh [open [file join $dir win tclWinReg.c] r]
        set fc [read $fh]
        close $fh

        if {[regexp -line "define\\s+TCL_REG_VERSION\\s+\"(.*?)\"" $fc - regver]} {
        }  elseif {[regexp -line "Tcl_PkgProvide\\s*\\(.*?, .*?, \"(.*?)\"" $fc - regver]} {
        }  else  {
            error "Unable to determine DDE version"
        }

        lappend filelistcustom lib/reg$regver/pkgIndex.tcl data "package ifneeded registry \"$regver\" \{load \"\" registry\}" ""
        lappend filelistcustom lib/dde$ddever/pkgIndex.tcl data "package ifneeded dde \"$ddever\" \{load \"\" dde\}" ""
    }
    return [concat $filelist $filelist1 $filelist2 $filelistcustom]
}

proc cookit::tcl::linkerflags-static {} {
    cookit::loadConfigFile tcl conf

    set rc [list]
    set rc [concat $rc $conf(TCL_LIBS) $conf(TCL_LD_FLAGS)]
    return $rc
}

proc cookit::tcl::linkerfiles-static {} {
    cookit::loadConfigFile tcl conf

    set dir [cookit::getBuildStaticDirectory tcl]

    set rc [list]
    lappend rc [cookit::wdrelative [file join $dir $conf(TCL_LIB_FILE)]]
    if {$::cookit::platform == "win32-x86"} {
        set g [glob -nocomplain -directory $dir "{libtclreg,tclreg}*.a"]
        if {[llength $g] != 1} {error "Unable to find registry package archive"}
        lappend rc [cookit::wdrelative [lindex $g 0]]

        set g [glob -directory $dir "{libtcldde,tcldde}*.a"]
        if {[llength $g] != 1} {error "Unable to find DDE package archive"}
        lappend rc [cookit::wdrelative [lindex $g 0]]
    }
    lappend rc [cookit::wdrelative [file join [cookit::getInstallStaticDirectory] lib $conf(TCL_STUB_LIB_FILE)]]
    return $rc
}

proc cookit::tcl::initialize-dynamic {version} {
}

proc cookit::tcl::configure-dynamic {} {
    cookit::buildConfigure -platform -mode dynamic -sourcepath relative
}

proc cookit::tcl::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::tcl::install-dynamic {} {
    cookit::buildMake install-binaries
    cookit::buildMake install-libraries

    set tclsh [glob -directory [file join [cookit::getInstallDynamicDirectory] bin] -type f tclsh*]

    file copy -force [lindex $tclsh 0] [file join [cookit::getInstallDynamicDirectory] bin tclkit]
}

proc cookit::tcl::packageslist-dynamic {} {
    return [list]
}

