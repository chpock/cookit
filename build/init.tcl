namespace eval cookit {}

package require cmdline

set cookit::allOptions(buildvariant.arg) {"" {Build variant to use for build directory prefixes}}
set cookit::allOptions(outputvariant.arg) {"" {Build variant to use for output directory prefixes}}

proc cookit::parseArguments {} {
    global argv
    variable allOptions
    variable commandInfo
    variable opt

    set options [list]
    foreach n [lsort [array names allOptions]] {
        lappend options [linsert $allOptions($n) 0 $n]
    }
    set usage "?options? command ?argument1? ?..? ?argumentN?\n"
    append usage "\nAvailable commands:\n"
    foreach n [lsort [array names commandInfo]] {
        set u "  $n [lindex $commandInfo($n) 0]"
        if {[string length $u] <= 20} {
            append usage [format "%-22s%s\n" $u [lindex $commandInfo($n) 1]]
        }  else  {
            append usage $u "\n                      [lindex $commandInfo($n) 1]\n"
        }
    }
    append usage "\nOptions:"

    if {[catch {
        array set opt [cmdline::getoptions argv $options $usage]
    } usagetext]} {
        puts stderr $usagetext
        exit 1
    }

    if {[llength $argv] < 1} {
        set argv {-help}
        if {[catch {
            array set opt [cmdline::getoptions argv $options $usage]
        } usagetext]} {
            puts stderr $usagetext
            exit 1
        }
    }

    # TODO: verify part versions
}

proc cookit::runCommand {} {
    global argv

    set command [lindex $argv 0]
    set argv [lrange $argv 1 end]

    if {[catch [linsert $argv 0 cmd[string totitle $command]] error]} {
        if {[info exists ::env(COOKITBUILDDEBUG)]} {
            puts stderr $::errorInfo
        }  else  {
            puts stderr $error
        }
	exit 1
    }
}

proc cookit::initDirectories {} {
    variable rootdirectory
    variable hostname
    variable hostbuilddirectory
    variable platform
    variable unixwinplatform
    variable opt
    variable stderrfilename
    variable installstaticdirectory
    variable installdynamicdirectory
    variable tempdirectory
    variable outputdirectory
    variable toolsdirectory

    if {$opt(buildvariant) != ""} {
        set hostbuilddirectory "$hostname-$opt(buildvariant)"
    }  else  {
        set hostbuilddirectory $hostname
    }

    if {$opt(outputvariant) != ""} {
        set outputdirectory [file join $rootdirectory _output $platform-$opt(outputvariant)]
    }  else  {
        set outputdirectory [file join $rootdirectory _output $platform]
    }

    set toolsdirectory [file join $rootdirectory tools]

    set installstaticdirectory [file join $rootdirectory _install_static $hostbuilddirectory]
    set installdynamicdirectory [file join $rootdirectory _install_dynamic $hostbuilddirectory]
    set tempdirectory [file join $rootdirectory _temp $hostbuilddirectory]

    set stderrfilename [file join $rootdirectory _temp $hostbuilddirectory __stderr__.txt]

    foreach directory [list sources \
        [file join _build_static $hostbuilddirectory] \
        [file join _build_dynamic $hostbuilddirectory] \
        [file join _install_static $hostbuilddirectory] \
        [file join _install_dynamic $hostbuilddirectory] \
        [file join _temp $hostbuilddirectory] \
        $outputdirectory \
        [file join $outputdirectory packages] \
        [file join _log $hostbuilddirectory] \
        ] {
        if {![file exists [file join $rootdirectory $directory]]} {
            file mkdir [file join $rootdirectory $directory]
        }
    }
}

proc cookit::getTempDirectory {} {
    variable tempdirectory
    set prefix "temp-[pid]-"
    while {1} {
        set dirname ${prefix}[expr {int(rand() * 0x7fffffff)}]
        set tempdir [file join $tempdirectory $dirname]
        if {[file exists $tempdir]} {continue}
        break
    }
    file mkdir $tempdir
    return $tempdir
}

proc cookit::initPartBuild {buildmode name version} {
    variable sourcedirectory
    variable buildstaticdirectory
    variable builddynamicdirectory
    variable installstaticdirectory
    variable installdynamicdirectory
    variable hostbuilddirectory
    variable rootdirectory

    set sourcedirectory [getSourceDirectory $name $version]
    set buildstaticdirectory [file join $rootdirectory _build_static $hostbuilddirectory $name]
    set builddynamicdirectory [file join $rootdirectory _build_dynamic $hostbuilddirectory $name]

    log 5 "Initializing $buildmode build for $name ($version)"
    if {![file exists $buildstaticdirectory]} {
        log 5 "Creating directory $buildstaticdirectory"
        file mkdir $buildstaticdirectory
    }

    if {![file exists $builddynamicdirectory]} {
        log 5 "Creating directory $builddynamicdirectory"
        file mkdir $builddynamicdirectory
    }

    if {![file exists $installdynamicdirectory]} {
        log 5 "Creating directory $installdynamicdirectory"
        file mkdir $installdynamicdirectory
    }

    if {![file exists $installstaticdirectory]} {
        log 5 "Creating directory $installstaticdirectory"
        file mkdir $installstaticdirectory
    }

    if {(![file exists $sourcedirectory]) || ([llength [glob -directory $sourcedirectory -nocomplain *]] == 0)} {
        log 3 "Extracting package $name from source tarball"
        set ok 0
        file mkdir $sourcedirectory
        foreach g [glob -nocomplain -directory [getDownloadsDirectory] $name-$version.*] {
            if {[string match *.tar.gz [file tail $g]] || [string match *.tgz [file tail $g]]} {
                log 4 "Extracting package $name from [file tail $g]"
                set pwd [pwd]
                cd $sourcedirectory
                if {[catch {exec tar -xzf [mkrelative [pwd] $g]} err]} {
		    log 2 "Extracting package failed: $err"
		}  else  {
		    set ok 1
		}
                break
            }
        }
        if {$ok} {
            set g0 [glob -nocomplain -directory . *]
            if {[llength $g0] == 1} {
                log 5 "Moving all entries from [file tail $g0] subdirectory to source directory"
                foreach g [glob -directory $g0 *] {
                    file rename $g [file tail $g]
                }
                file delete $g0
            }
            cd $pwd
        }  else  {
            error "No valid file found"
        }
    }

    cookit::${name}::initialize-$buildmode $version

    if {$buildmode == "static"} {
        cd $buildstaticdirectory
    }  else  {
        cd $builddynamicdirectory
    }
}

proc cookit::buildDirectory {name} {
    variable rootdirectory
    variable hostbuilddirectory
    return [file join $rootdirectory _build $hostbuilddirectory $name]
}


