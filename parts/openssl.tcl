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
    # TODO: clean up OSX here
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

proc cookit::openssl::_updatecflags {} {
    upvar 1 origcflags origcflags
    if {[info exists ::env(CFLAGS)]} {
        set flags ""
        set origcflags $::env(CFLAGS)
        set doskip 1
        foreach f $::env(CFLAGS) {
            if {[string match "-arch" $f]} {
                set doskip 1
            }  elseif  {$doskip} {
                set doskip 0
            }  else  {
                lappend flags $f
            }
        }
        set ::env(CFLAGS) $flags
    }
}

proc cookit::openssl::configure-dynamic {} {
    cookit::log 3 "cookit::openssl::configure-dynamic: Syncing up OpenSSL sources"
    _copysource

    if {[string match "macosx-universal*" $::cookit::platform]} {
        set wd [pwd]
	set buildppc [string match "*arch ppc*" $::env(CFLAGS)]
	set buildx64 [string match "*arch x86_64*" $::env(CFLAGS)]

        set builds [list $wd openssl darwin-i386-cc]

	if {$buildppc} {
	    file mkdir _ppc
	    cookit::log 3 "cookit::openssl::configure-dynamic: Syncing up OpenSSL sources for PPC build"
	    foreach g [glob -directory [pwd] -tails *] {
		if {$g != "_ppc"} {
		    catch {file delete -force [file join _ppc $g]}
		    file copy -force $g [file join _ppc $g]
		}
            }
            lappend builds [file join $wd _ppc] openssl_ppc darwin-ppc-cc
        }
	if {$buildx64} {
	    file mkdir _x64
	    cookit::log 3 "cookit::openssl::configure-dynamic: Syncing up OpenSSL sources for x64 build"
	    foreach g [glob -directory [pwd] -tails *] {
		if {$g != "_x64"} {
		    catch {file delete -force [file join _x64 $g]}
		    file copy -force $g [file join _x64 $g]
		}
            }
            lappend builds [file join $wd _x64] openssl_x64 darwin64-x86_64-cc
        }

        _updatecflags

        foreach {dir instdir platform} $builds {
            cookit::log 3 "cookit::openssl::configure-dynamic: Configuring OpenSSL ($dir) for $platform"
            cd $dir
            set cmd [list perl ./Configure $platform]
            lappend cmd --prefix=[file join [cookit::getInstallDynamicDirectory] openssl]
            lappend cmd no-shared
            lappend cmd no-zlib

            set eid [cookit::startExtCommand $cmd]
            cookit::waitForExtCommand $eid 0

            # add CFLAGS to Makefile
            set fh [open Makefile r]
            fconfigure $fh -translation auto -encoding iso8859-1
            set fc [read $fh]
            close $fh
            regsub -line "CFLAG= " $fc "CFLAG= \$(CFLAGS) " fc
            set fh [open Makefile w]
            fconfigure $fh -translation lf -encoding iso8859-1
            puts -nonewline $fh $fc
            close $fh
        }

        cd $wd
        if {[info exists origcflags]} {
            set ::env(CFLAGS) $origcflags
        }
    }  else  {
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
        }  elseif {$::cookit::platform == "linux-x64"} {
            set cmd [list ./Configure linux-x86_64]
            lappend cmd no-asm -fPIC
        }  elseif {$::cookit::platform == "linux-x86"} {
            set cmd [list ./Configure linux-elf]
            lappend cmd no-asm
        }  elseif {$::cookit::platform == "macosx-x86"} {
            set cmd [list ./Configure darwin-i386-cc]
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
}

proc cookit::openssl::build-dynamic {} {
    if {[string match "macosx-universal*" $::cookit::platform]} {
        set wd [pwd]
	set buildppc [string match "*arch ppc*" $::env(CFLAGS)]
	set buildx64 [string match "*arch x86_64*" $::env(CFLAGS)]
        set builds [list $wd openssl darwin-i386-cc]
	if {$buildppc} {
            lappend builds [file join $wd _ppc] openssl_ppc darwin-ppc-cc
        }
	if {$buildx64} {
            lappend builds [file join $wd _x64] openssl_x64 darwin64-x86_64-cc
        }

        _updatecflags
        foreach {dir instdir platform} $builds {
            cd $dir
            cookit::buildMake all
        }
        cd $wd
        if {[info exists origcflags]} {
            set ::env(CFLAGS) $origcflags
        }
    }  else  {
            cookit::buildMake all
    }
}

proc cookit::openssl::install-dynamic {} {
    if {[string match "macosx-universal*" $::cookit::platform]} {
        set builddir [pwd]
        set installdir [file join [cookit::getInstallDynamicDirectory] openssl]

	set buildppc [string match "*arch ppc*" $::env(CFLAGS)]
	set buildx64 [string match "*arch x86_64*" $::env(CFLAGS)]

        _updatecflags
        cookit::buildMake install
        if {[info exists origcflags]} {
            set ::env(CFLAGS) $origcflags
        }

        foreach lib {libssl libcrypto} {
	    set filelist [list [file join $builddir $lib.a]]
	    if {$buildppc} {
		lappend filelist [file join $builddir _ppc $lib.a]
	    }
	    if {$buildx64} {
		lappend filelist [file join $builddir _x64 $lib.a]
	    }
	    cookit::log 3 "cookit::openssl::install-dynamic: Combining $lib.a using $filelist"
            combineArFiles [file join $installdir lib $lib.a] $filelist
        }
    }  else  {
        cookit::buildMake install
    }
}

proc cookit::openssl::packageslist-dynamic {} {
    return [list]
}

proc cookit::openssl::combineArFiles {output filelist} {
    set output [file normalize $output]
    set id 0
    foreach f $filelist {
        lappend filist [file normalize $f] [incr id]
    }

    set wd [pwd]
    set tempdir [file join $::cookit::tempdirectory ar-[pid]]
    catch {file delete -force $tempdir}
    file mkdir $tempdir

    cd $tempdir

    set allfilelist [list]
    foreach {f id} $filist {
        cd $tempdir
        file mkdir $id
        cd $id
        cookit::log 4 "Unpacking $f in [pwd]"

        set eid [cookit::startExtCommand [list ar -x $f]]
        cookit::waitForExtCommand $eid 0

        set fl {}
        foreach f [glob -nocomplain -type f * */* */*/* */*/*/* */*/*/*/* */*/*/*/*/* */*/*/*/*/*/*] {
            if {[string match *.o $f]} {
                lappend fl $f
            }
        }
        set allfilelist [lsort -unique [concat $allfilelist $fl]]
    }

    cd $tempdir
    file mkdir out

    foreach f $allfilelist {
        set srcfiles [list]
        foreach {- id} $filist {
            set sf [file normalize [file join $id $f]]
            if {[file exists $sf]} {
                lappend srcfiles $sf
            }
        }
        cookit::log 5 "Combining file $f using $srcfiles"
        catch {file mkdir [file join out [file dirname $f]]}
        set cmd [list lipo -create -output out/$f]
        set cmd [concat $cmd $srcfiles]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }

    cd out
    cookit::log 3 "Creating new archive file - $output"
    file delete -force $output
    set cmd [list ar cr $output]
    set cmd [concat $cmd $allfilelist]

    set eid [cookit::startExtCommand $cmd]
    cookit::waitForExtCommand $eid 0

    set cmd [list ranlib $output]
    set eid [cookit::startExtCommand $cmd]
    cookit::waitForExtCommand $eid 0

    cd $wd
    catch {file delete -force $tempdir}
}

