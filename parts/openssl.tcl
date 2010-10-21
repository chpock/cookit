namespace eval cookit::openssl {}

cookit::partRegister openssl "CooKit VFS"

proc cookit::openssl::_copysource {} {
    if {[info exists ::env(COOKITSKIPOPENSSLCOPY)]} {
        cookit::log 3 "cookit::openssl::_copysource: Skipping copying of OpenSSL"
        return
    }
    set sourcedir [cookit::getSourceDirectory openssl]
    catch {eval file delete -force [glob -nocomplain *]}
    cookit::writeFilelist [pwd] \
        [cookit::listAllFiles "" $sourcedir]
}

proc cookit::openssl::retrievesource {} {
    set url http://www.openssl.org/source/
    set html [cookit::downloadURL $url]
    if {![regexp -line "^.*\\\[LATEST\\\].*\$" $html line]} {
        error "Unable to download openssl sources - unable to find LATEST label"
    }
    if {![regexp "<a\\s+?href=\"(.*?)\"" $line - filename]} {
        error "Unable to download openssl sources - no URL on website"
    }
    set fileurl "$url$filename"
    set destfile [file join [cookit::getDownloadsDirectory] $filename]

    cookit::downloadURL $fileurl $destfile
}

proc cookit::openssl::parameters {version} {
    return [list \
        provides {} depends {} \
	buildmodes {static dynamic} \
        ]
}

proc cookit::openssl::initialize-static {version} {
}

proc cookit::openssl::configure-static {} {
    cookit::log 3 "cookit::openssl::configure-static: Syncing up OpenSSL sources"
    _copysource

    cookit::log 3 "cookit::openssl::configure-static: Configuring OpenSSL"
    if {$::cookit::platform == "win32-x86"} {
if {0} {
        set fh [open Configure r]
        fconfigure $fh -translation binary
        set configurefc [read $fh]
        close $fh

        regsub -line \
            "\\\$IsMK1MF=1 if \\(\\\$target eq \"mingw\" \\&\\& \\\$\\^O ne \"cygwin\" \\&\\& \\!is_msys\\(\\)\\);" \
            $configurefc "" configurefc

        set fh [open Configure w]
        fconfigure $fh -translation binary
        puts -nonewline $fh $configurefc
        close $fh
}

        # on Windows, run perl script directly
        set cmd [list perl ./Configure]
        lappend cmd mingw
    
    }  elseif {$::cookit::platform == "solaris-x86"} {
        set cmd [list ./Configure solaris-x86-gcc]
        lappend cmd no-asm
    }  elseif {$::cookit::platform == "linux-x86"} {
        set cmd [list ./Configure linux-elf]
        lappend cmd no-asm
    }  else  {
        # on Unix, run the config command
        set cmd [list ./config]
    }

    lappend cmd --prefix=[file join [cookit::getInstallStaticDirectory] openssl]
    lappend cmd no-shared
    #lappend cmd no-zlib

    set eid [cookit::startExtCommand $cmd]
    cookit::waitForExtCommand $eid 0
}

proc cookit::openssl::build-static {} {
    cookit::buildMake all
}

proc cookit::openssl::install-static {} {
    cookit::buildMake install
}

proc cookit::openssl::vfsfilelist-static {} {
    set filelist [list]
    return $filelist
}

proc cookit::openssl::linkerflags-static {} {
    return [list]
}

proc cookit::openssl::linkerfiles-static {} {
    set ssldir [file join [cookit::getInstallStaticDirectory] openssl]
    set rc [list]
    lappend rc [file join $ssldir lib libssl.a]
    lappend rc [file join $ssldir lib libcrypto.a]
    return $rc
}

proc cookit::openssl::initialize-dynamic {version} {
}

proc cookit::openssl::configure-dynamic {} {
    cookit::log 3 "cookit::openssl::configure-dynamic: Syncing up OpenSSL sources"
    _copysource

    cookit::log 3 "cookit::openssl::configure-dynamic: Configuring OpenSSL"
    if {$::cookit::platform == "win32-x86"} {
        set fh [open Configure r]
        fconfigure $fh -translation binary
        set configurefc [read $fh]
        close $fh

        regsub -line \
            "\\\$IsMK1MF=1 if \\(\\\$target eq \"mingw\" \\&\\& \\\$\\^O ne \"cygwin\" \\&\\& \\!is_msys\\(\\)\\);" \
            $configurefc "" configurefc

        set fh [open Configure w]
        fconfigure $fh -translation binary
        puts -nonewline $fh $configurefc
        close $fh

        # on Windows, run perl script directly
        set cmd [list perl ./Configure]
        lappend cmd mingw
    
    }  elseif {$::cookit::platform == "solaris-x86"} {
        set cmd [list ./Configure solaris-x86-gcc]
        lappend cmd no-asm
    }  elseif {$::cookit::platform == "linux-x86"} {
        set cmd [list ./Configure linux-elf]
        lappend cmd no-asm
    }  else  {
        # on Unix, run the config command
        set cmd [list ./config]
    }

    lappend cmd --prefix=[file join [cookit::getInstallDynamicDirectory] openssl]
    lappend cmd no-shared
    lappend cmd no-zlib

    set eid [cookit::startExtCommand $cmd]
    cookit::waitForExtCommand $eid 0
}

proc cookit::openssl::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::openssl::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::openssl::packageslist-dynamic {} {
    return [list]
}
