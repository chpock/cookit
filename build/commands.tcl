namespace eval cookit {}

set cookit::allOptions(binarysuffix.arg) {{} {Suffix to append to binary name}}
set cookit::allOptions(upx.arg) {none {UPX compression to use}}
set cookit::allOptions(upxbinary.arg) {upx {Override UPX binary to use}}
set cookit::allOptions(skip-vfsrepack) {{Skip VFS repacking - produces larger binary, only useful for testing}}
#
# parts
#

proc cookit::cmdPartClean {all} {
    variable rootdirectory
    variable hostbuilddirectory

    logInit _clean

    set steps [list \
        "Clean up installed static files" \
        "Clean up installed dynamic files" \
        "Clean up build static directory" \
        "Clean up build dynamic directory" \
        "Clean up temporary directory" \
        "Clean up logs" \
        ]
    
    uiInitializeProgress $steps

    if {$all} {
        set pattern "*"
    }  else  {
        set pattern $hostbuilddirectory
    }

    foreach dir [list _install_static _install_dynamic _build_static _build_dynamic _temp] {
        uiNextStep

        foreach dir [glob -nocomplain -directory [file join $rootdirectory $dir] $pattern] {
            log 3 "cmdPartClean: Cleaning up directory \"$dir\""
            uiAddItem "Cleaning up \"[file tail $dir]\""
            foreach g [glob -nocomplain -directory $dir *] {
                uiAddItem "Cleaning up directory [file tail $g]"
                log 5 "cmdPartClean: Cleaning up entry \"$g\""
                if {[catch {
                    file delete -force $g
                } error]} {
                    log 2 "cmdPartClean: Unable to delete \"$g\": $error"
                }
            }
            if {[llength [glob -nocomplain -directory $dir *]] == 0} {
                log 5 "cmdPartClean: Removing empty directory \"$dir\""
                if {[catch {
                    file delete -force $dir
                } error]} {
                    log 2 "cmdPartClean: Unable to delete \"$g\": $error"
                }
            }
        }
    }
    
    uiNextStep
    foreach dir [glob -nocomplain -directory [file join $rootdirectory _log] $pattern] {
        log 3 "cmdPartClean: Cleaning up directory \"$dir\""
        uiAddItem "Cleaning up \"[file tail $dir]\""
        foreach g [glob -nocomplain -directory $dir *] {
            if {([file tail $dir] == $hostbuilddirectory) && ([file tail $g] == "_clean.txt")} {
                log 5 "cmdPartClean: Skipping entry \"$g\""
                continue
            }  else  {
                log 5 "cmdPartClean: Cleaning up entry \"$g\""
                if {[catch {
                    file delete -force $g
                } error]} {
                    log 2 "cmdPartClean: Unable to delete \"$g\": $error"
                }
            }
        }
        if {[llength [glob -nocomplain -directory $dir *]] == 0} {
            log 5 "cmdPartClean: Removing empty directory \"$dir\""
            if {[catch {
                file delete -force $dir
            } error]} {
                log 2 "cmdPartClean: Unable to delete \"$g\": $error"
            }
        }
    }

    uiComplete
}

# parts - static (cookit) builds
proc cookit::cmdPartCompileStatic {name version} {
    uiAddItem "Initializing"
    initPartBuild static $name $version
    uiAddItem "Configuring"
    cookit::runPartCommand $name configure-static
    uiAddItem "Compiling"
    cookit::runPartCommand $name build-static
    uiAddItem "Installing"
    cookit::runPartCommand $name install-static
    uiAddItem "Finalizing"
}

# parts - static (cookit) builds
proc cookit::cmdPartLink {plan} {
    variable platform
    variable opt
    
    uiAddItem "Initializing"
    cd [getOutputDirectory]
    set output "./cookit"

    if {$platform == "win32-x86"} {
        set outputsuffix ".exe"
    }  else  {
        set outputsuffix ""
    }

    if {$opt(binarysuffix) != ""} {
        set outputsuffix "-$opt(binarysuffix)$outputsuffix"
    }

    log 5 "Output base filename \"$output\"; suffix: \"$outputsuffix\""

    set output "$output$outputsuffix"

    log 3 "Output filename: $output"

    set gcc [cookit::osSpecific gcc]

    set command $gcc
    if {[info exists ::env(CFLAGS)]} {
        set command [concat $gcc $::env(CFLAGS)]
    }

    lappend command -o $output

    set filelist [list]
    set flags [list]
    set externcode ""
    set appinitcode ""

    foreach {name version} $plan {
        set filelist [concat \
            [cookit::runPartCommand $name linkerfiles-static] \
            $filelist]
        set flags [concat \
            [cookit::runPartCommand $name linkerflags-static] \
            $flags]
    }

    # if both -mwindows and -mconsole is present, use first one
    set oldflags $flags
    set flags [list]
    set mwindowsconsoledone 0
    set mflag [list]

    foreach flag $oldflags {
        if {$platform == "win32-x86"} {
            if {[lsearch -exact {-mwindows -mconsole} $flag] >= 0} {
                set mflag [list $flag]
                continue
            }
        }
        if {[regexp "^-\[lL\]" $flag]} {
            if {[info exists flagdone($flag)]} {
                continue
            }
            set flagdone($flag) 1
        }
        lappend flags $flag
    }
    set flags [concat $flags $mflag]

    log 4 "All flags: $flags"
    
    set command [concat $command $filelist $flags]

    uiAddItem "Linking"
    set eid [startExtCommand $command]
    waitForExtCommand $eid 0
    
    if {1} {
        uiAddItem "Stripping"
        set cmd [osSpecific strip]
        lappend cmd $output
        set eid [startExtCommand $cmd]
        waitForExtCommand $eid 0
    }

    if {$opt(upx) != "none"} {
        set cmd [lrange $opt(upxbinary) 0 end]
        uiAddItem "Compressing"
        if {$platform == "win32-x86"} {
            lappend cmd --compress-resources=0
        }
        switch -- $opt(upx) {
            ultra-brute {
                lappend cmd --best --$opt(upx) --8mib-ram
            }
            best - brute {
                lappend cmd --$opt(upx)
            }
            1 - 2 - 3 - 4 - 5 - 6 - 7 - 8 - 9 {
                lappend cmd -$opt(upx)
            }
            noopt {
            }
        }
        set fname $output
        if {[catch {
            set fname [file normalize $fname]
        }]} {
            set fname [file join [pwd] $fname]
        }
        lappend cmd $fname
        set eid [startExtCommand $cmd]
        waitForExtCommand $eid 0
    }
}

proc cookit::cmdPartAddVFS {plan} {
    variable rootdirectory
    variable platform
    variable opt
    
    uiAddItem "Initializing"
    cd [getOutputDirectory]
    set output [getCookitName]

    source [file join [getSourceDirectory cookfs] cookfswriter cookfswriter.tcl]

    set bootstrap {}
    set filelist [list]

    foreach {name version} $plan {
        set cmd ::cookit::${name}::vfsbootstrap
        if {[info commands $cmd] != ""} {
            append bootstrap [$cmd]
        }

        if {[checkPartCommandExists $name vfsfilelist-static]} {
            set filelist [concat $filelist \
                [lrange [cookit::runPartCommand $name vfsfilelist-static] 0 end] \
                ]
        }
    }

    log 3 "Building VFS based on filelist; [expr {[llength $filelist] / 4}] item(s)"

    uiAddItem "Adding temporary VFS"
    cookfs::createArchive $output $filelist $bootstrap
    log 5 "Bootstrap is [string length $bootstrap] byte(s)"

    if {!$opt(skip-vfsrepack)} {
        log 3 "Running temporary binary to repack VFS using full cookfs"

        uiAddItem "Re-compressing VFS"
        cfsRepack source $output destination $output.tmp bootstrap 1

        uiAddItem "Finalizing"
        log 3 "Reverting binaries"
        file copy -force $output.tmp $output
        file delete $output.tmp
    }
}
# parts - dynamic builds
proc cookit::cmdPartConfigureDynamic {name version} {
    uiAddItem "Initializing"
    initPartBuild dynamic $name $version
    uiAddItem "Configuring"
    cookit::runPartCommand $name configure-dynamic
}

# parts - dynamic builds
proc cookit::cmdPartMakeDynamic {name version} {
    uiAddItem "Compiling"
    cookit::runPartCommand $name build-dynamic
    uiAddItem "Installing"
    cookit::runPartCommand $name install-dynamic
    uiAddItem "Finalizing"
}

proc cookit::cmdPartPackagesDynamic {name version} {
    variable opt

    uiAddItem "Initializing"
    initPartBuild dynamic $name $version

    source [file join [getSourceDirectory cookfs] cookfswriter cookfswriter.tcl]

    if {[checkPartCommandExists $name packageslist-dynamic]} {
        uiAddItem "Listing packages"
        uiAddItem "Listing packages for $name $version"
        set packages [runPartCommand $name packageslist-dynamic]

        foreach {package filelist} $packages {
            uiAddItem "Creating package $package"

            set pkgfilename [file join [getOutputPackagesDirectory] $package.cfspkg]
            log 3 "Creating package $pkgfilename"

            # TODO: create file header?
            close [open $pkgfilename.tmp w]

            cookfs::createArchive $pkgfilename.tmp $filelist ""

            if {!$opt(skip-vfsrepack)} {
                log 3 "Running binary to repack CFS package"
                uiAddItem "Re-compressing package $package"

                cfsRepack source $pkgfilename.tmp destination $pkgfilename

                uiAddItem "Finalizing"
                log 5 "Removing temporary file"
                file delete -force $pkgfilename.tmp
            }
        }
    }
}

# parts - info
set cookit::commandInfo(info) {{} {Print information on platform}}
proc cookit::cmdInfo {args} {
    puts "Platform: $::cookit::platform"
    exit 0
}

# parts - cleanup
set cookit::commandInfo(clean) {{} {Clean up directories for this host and variant}}
proc cookit::cmdClean {args} {
    cmdPartClean 0
}


set cookit::commandInfo(cleanall) {{} {Clean up directories for all hosts and variants}}
proc cookit::cmdCleanall {args} {
    cmdPartClean 1
}

set cookit::commandInfo(retrievesource) {{<parts>} {Retrieve source code for one or more parts of cookit}}
proc cookit::cmdRetrievesource {args} {
    variable versions

    if {([llength $args] == 0) && ([llength $versions(tcl)] == 0) && ([llength $versions(cookfs)] == 0)} {
        # there are no known versions for basic packages and user did not specify
        # any parts to download so we need to ask which parts
        # should be downloaded in such case
        set parts [list]

        foreach part [lsort [array names versions]] {
            set parameters [cookit::${part}::parameters [lindex $versions($part) 0]]
            catch {unset partparam}
            array set partparam $parameters
            if {[info exists partparam(retrieveByDefault)] && $partparam(retrieveByDefault)} {
                lappend parts $part
            }
        }
        catch {unset partparam}
    }  elseif {[llength $args] == 0} {
        # there are known versions and user did not specify any parts to download
        # so we analyze known items and decide which would go to a cookit
        set plan [createSolution "" virtual:cookit]
        array set planArray $plan
        setPartVersions $plan
        initPartVersions static $plan

        set parts [list]
        foreach part [lsort [array names versions]] {
            if {[info commands ::cookit::${part}::retrievesource] == ""} {
                continue
            }
            if {[info exists planArray($part)]} {
                lappend parts $part
            }
        }
    }  else  {
        set parts $args
    }
    
    set steps [list]
    foreach part $parts {
        lappend steps "Retrieve source code for $part"
    }

    uiInitializeProgress $steps

    foreach part $parts {
        logInit $part-retrieve

        uiNextStep

        uiAddItem "Retrieving"
        if {[catch {
            cookit::${part}::retrievesource
        } error]} {
            uiAddItem "Failed: $error"
            # TODO: log
        }  else  {
            uiAddItem "Success"
        }
    }

    uiComplete
}

# parts - cookit building
set cookit::commandInfo(build-cookit) {{} {Builds a CooKit with predefined criteria}}
proc cookit::cmdBuild-cookit {} {
    set plan [createSolution static virtual:cookit]

    setPartVersions $plan
    initPartVersions static $plan

    foreach {pname pversion} $plan {
        lappend steps "Build $pname $pversion"
    }

    lappend steps "Link binary" "Add CookFS layer"

    uiInitializeProgress $steps

    foreach {pname pversion} $plan {
        logInit $pname-static
        uiNextStep
        cmdPartCompileStatic $pname $pversion
    }

    # linking
    uiNextStep
    logInit _cookit_link
    cmdPartLink $plan

    # finalizing
    uiNextStep
    logInit _cookit_vfs
    cmdPartAddVFS $plan
    
    uiComplete
}

set cookit::commandInfo(binary-cookit) {{} {Link and finalize CooKit binary}}
proc cookit::cmdBinary-cookit {} {
    set plan [createSolution static virtual:cookit]

    setPartVersions $plan
    initPartVersions static $plan

    lappend steps "Link binary" "Add CookFS layer"

    uiInitializeProgress $steps

    # linking
    uiNextStep
    logInit _cookit_link
    cmdPartLink $plan

    # finalizing
    uiNextStep
    logInit _cookit_vfs
    if {[catch {
        cmdPartAddVFS $plan
    }]} {
        puts $::errorInfo
        exit 1
    }
    
    uiComplete
}

set cookit::commandInfo(binary-link-cookit) {{} {Link and finalize CooKit binary}}
proc cookit::cmdBinary-link-cookit {} {
    set plan [createSolution static virtual:cookit]

    setPartVersions $plan
    initPartVersions static $plan

    lappend steps "Link binary"

    uiInitializeProgress $steps

    # linking
    uiNextStep
    logInit _cookit_link
    cmdPartLink $plan

    uiComplete
}

set cookit::commandInfo(binary-vfs-cookit) {{} {Link and finalize CooKit binary}}
proc cookit::cmdBinary-vfs-cookit {} {
    set plan [createSolution static virtual:cookit]

    setPartVersions $plan
    initPartVersions static $plan

    lappend steps "Add CookFS layer"

    uiInitializeProgress $steps

    # finalizing
    uiNextStep
    logInit _cookit_vfs
    if {[catch {
        cmdPartAddVFS $plan
    }]} {
        puts $::errorInfo
        exit 1
    }
    
    uiComplete
}

set cookit::commandInfo(compile-cookit) {{<parts>} {Compiles particular parts of CooKit binary}}
proc cookit::cmdCompile-cookit {args} {
    variable opt
    variable versions

    set plan [createSolution static virtual:cookit]

    setPartVersions $plan
    initPartVersions static $plan

    set steps [list]
    foreach part $args {
        set version [getPartVersion $part]
        lappend steps "Build $part $version"
    }
    
    uiInitializeProgress $steps

    foreach part $args {
        uiNextStep
        logInit $part-static
        set version [getPartVersion $part]
        cmdPartCompileStatic $part $version
    }
    
    uiComplete
}

# dynamic packages building
set cookit::commandInfo(build-dynamic) {{<parts>} {Compiles and creates embedable packages}}
proc cookit::cmdBuild-dynamic {args} {
    variable opt
    variable versions

    set plan [createSolution dynamic $args]

    setPartVersions $plan
    initPartVersions dynamic $plan

    set steps [list]
    foreach {part -} $plan {
        set version [getPartVersion $part]
        lappend steps "Packages for $part $version"
    }
    
    uiInitializeProgress $steps

    foreach {part -} $plan {
        uiNextStep
        logInit $part-dynamic
        set version [getPartVersion $part]
        cmdPartConfigureDynamic $part $version
        cmdPartMakeDynamic $part $version
        cmdPartPackagesDynamic $part $version
    }
    
    uiComplete
}

set cookit::commandInfo(compile-dynamic) {{<parts>} {Compiles dynamically available extensions}}
proc cookit::cmdCompile-dynamic {args} {
    variable opt
    variable versions

    set plan [createSolution dynamic $args]

    setPartVersions $plan
    initPartVersions dynamic $plan

    set steps [list]
    foreach part $args {
        set version [getPartVersion $part]
        lappend steps "Build $part $version"
    }
    
    uiInitializeProgress $steps

    foreach part $args {
        uiNextStep
        logInit $part-dynamic
        set version [getPartVersion $part]
        cmdPartConfigureDynamic $part $version
        cmdPartMakeDynamic $part $version
    }
    
    uiComplete
}

set cookit::commandInfo(make-dynamic) {{<parts>} {Compiles dynamically available extensions}}
proc cookit::cmdMake-dynamic {args} {
    variable opt
    variable versions

    set plan [createSolution dynamic $args]

    setPartVersions $plan
    initPartVersions dynamic $plan

    set steps [list]
    foreach part $args {
        set version [getPartVersion $part]
        lappend steps "Build $part $version"
    }
    
    uiInitializeProgress $steps

    foreach part $args {
        uiNextStep
        logInit $part-dynamic
        set version [getPartVersion $part]
        cmdPartMakeDynamic $part $version
    }
    
    uiComplete
}

set cookit::commandInfo(packages-dynamic) {{<parts>} {Builds embedable packages from already compiled extensions}}
proc cookit::cmdPackages-dynamic {args} {
    variable opt
    variable versions

    set plan [createSolution dynamic $args]

    setPartVersions $plan
    initPartVersions dynamic $plan

    set steps [list]
    foreach part $args {
        set version [getPartVersion $part]
        lappend steps "Packages for $part $version"
    }
    
    uiInitializeProgress $steps

    foreach part $args {
        uiNextStep
        logInit $part-dynamic
        set version [getPartVersion $part]
        cmdPartPackagesDynamic $part $version
    }
    
    uiComplete
}

proc cookit::cmdScript {script args} {
    set ::argv0 $script
    set ::argv $args
    set ::argc [llength $::argv]
    if {[catch [list uplevel #0 [list source $::argv0]]]} {
        puts $::errorInfo
    }
}

