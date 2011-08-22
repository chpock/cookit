namespace eval cookit {}

proc cookit::initParts {} {
    foreach filename [lsort [glob -nocomplain -directory [file join $cookit::rootdirectory] parts/*.tcl]] {
        uplevel #0 [list source $filename]
    }
}

proc cookit::getParameters {type} {
    variable partparameters
    if {![info exists partparameters($type)]} {
        set partparameters($type) ""
    }
    return $partparameters($type)
}

proc cookit::addParameters {type string {uniqueid ""}} {
    variable partparameters
    variable partparametersdone
    if {![info exists partparameters($type)]} {
        set partparameters($type) ""
    }
    if {($uniqueid != "") && [info exists partparametersdone($type,$uniqueid)]} {
        return
    }
    set partparametersdone($type,$uniqueid) 1
    append partparameters($type) $string \n
}

proc cookit::addParametersList {type args} {
    variable partparameters
    if {![info exists partparameters($type)]} {
        set partparameters($type) [list]
    }
    set partparameters($type) [concat $partparameters($type) $args]
}

# comparison of two version strings -
# not using [package vcompare] to allow older
# Tcl versions to work
proc cookit::compareVersionsConvert {a} {
    if {[regexp "^(.*?)(a|b)(\[0-9\]+)(.*)\$" $a - v ab c d]} {
        set t "$v. $ab$c$d"
    }  elseif {[regexp "^(.*?)\\.(\[0-9\]+)\$" $a - v c]} {
        set t "$v.$c"
    }  else  {
        set t $a
    }
    set rc ""
    foreach t [split $t .] {
        append rc [format %-8s $t]
    }
    return $rc
}

# a>b -> >0
# a<b -> <0
proc cookit::compareVersions {a b} {
    return [string compare [compareVersionsConvert $a] [compareVersionsConvert $b]]
}

proc cookit::fixPartVersions {} {
    variable opt
    variable versions
    
    foreach n [array names versions] {
        if {[info exists opt($n)]} {
            if {($opt($n) == ".") || ($opt($n) == "latest")} {
                # TODO: make sure we actually get last, not first version
                set opt($n) [lindex $versions($n) end]
            }
        }
    }
}

proc cookit::partVersions {name} {
    variable rootdirectory
    variable partVersionsCache

    if {![info exists partVersionsCache($name)]} {
        if {[checkPartCommandExists $name getVersions]} {
            set rc [runPartCommand $name getVersions]
        }  else  {
            set rc [list]
            foreach g [concat \
                [glob -nocomplain -directory [file join $rootdirectory downloads] ${name}-*] \
                [glob -nocomplain -directory [file join $rootdirectory sources] ${name}-*] \
                ] {
                set g [file tail $g]
                if {[regexp "${name}-(.*?)\$" $g - v]} {
                    regsub "\.(tar\.gz|tgz|tar\.bz2|zip)\$" $v "" v
                    lappend rc $v
                }
            }
        }
        set partVersionsCache($name) [lsort -unique -command cookit::compareVersions $rc]
    }
    return $partVersionsCache($name)
}

proc cookit::partRegister {args} {
    variable allParts
    variable allLabels
    variable allOptions
    variable versions

    set options {
        {versions.arg           {AUTO}          {List of available versions}}
    }
    set usage "?options? name label"

    if {[catch {
        array set opt [cmdline::getoptions args $options $usage]
    } usagetext]} {
        return -code error -errorinfo $usagetext $usagetext
    }

    if {[llength $args] != 2} {
        set args -help
        if {[catch {
            array set opt [cmdline::getoptions args $options $usage]
        } usagetext]} {
            return -code error -errorinfo $usagetext $usagetext
        }
    }

    set name [lindex $args 0]
    set label [lindex $args 1]
    set allLabels($name) $label

    if {$opt(versions) != "AUTO"} {
        set versions($name) $opt(versions)
    }

    lappend allParts $name
    set allParts [lsort -unique $allParts]

    foreach name $allParts {
        # TODO: add option to list all versions
        set allOptions($name.arg) [list "" "Version of $allLabels($name) to use"]
    }
}

proc cookit::initAvailablePartVersions {} {
    variable allParts
    variable versions

    set ok 0
    for {set attempt 0} {$attempt < 10} {incr attempt} {
        set aok 1
        set failedparts [list]
        foreach name $allParts {
            if {![info exists versions($name)]} {
                if {[catch {
                    set versions($name) [partVersions $name]
                }]} {
                    set aok 0
                    lappend failedparts $name
                }
            }
        }
        if {$aok} {
            set ok 1
            break
        }
    }

    if {!$ok} {
        error "Failed to determine version of parts: $failedparts"
    }
}

proc cookit::partInitializeAllVersions {} {
    variable versions
    variable partParameters
    variable partProviders

    foreach name [lsort [array names versions]] {
        set partParameters($name) [list]
        foreach version $versions($name) {
            # TODO: apply defaults
            catch {unset params}
            array set params {provides {} depends {} buildmode static}
            array set params [cookit::${name}::parameters $version]
            set partParameters($name.$version) [array get params]

            lappend partProviders($name) $version $name $version
            foreach {vpackage packageversion} $params(provides) {
                lappend partProviders($vpackage) $packageversion $name $version
            }
        }
    }
}

proc cookit::partGetParameter {name version param} {
    variable partParameters
    array set a $partParameters($name.$version)
    return $a($param)
}

proc cookit::setPartVersions {list} {
    variable partVersion
    array set partVersion $list
}

proc cookit::initPartVersions {buildmode list} {
    variable partbuildmode

    foreach {name version} $list {
        cookit::${name}::initialize-$buildmode $version
        #set partbuildmode($name) [partGetParameter $name $version buildmode]
        set partbuildmode($name) $buildmode
    }
}

proc cookit::isPartIncluded {name} {
    variable partVersion

    if {[info exists partVersion($name)] && ($partVersion($name) != "")} {
        return 1
    }  else  {
        return 0
    }
}

proc cookit::getPartVersion {name} {
    variable opt
    variable versions
    variable partVersion

    if {[info exists partVersion($name)]} {
        return $partVersion($name)
    }
    set version $opt($name)
    if {$version == ""} {
        set version [lindex $versions($name) end]
    }
    return $version
}

proc cookit::getPartConfigFilename {name} {
    variable partbuildmode
    if {[catch {partGetParameter $name [getPartVersion $name] configfile} filename]} {
        return ""
    }  elseif  {$filename == ""} {
        return ""
    }

    if {$partbuildmode($name) == "static"} {
        return [file join [getBuildStaticDirectory $name] $filename]
    }  else  {
        return [file join [getBuildDynamicDirectory $name] $filename]
    }
}

proc cookit::loadConfigFile {name arrayname} {
    upvar 1 $arrayname a
    set fn [getPartConfigFilename $name]
    log 3 "getConfig: Loading $name configuration file: \"$fn\""
    if {$fn != ""} {
        array set a [loadRawConfigFile $fn [array get a]]
    }
}

proc cookit::loadRawConfigFile {filename values} {
    variable platform
    set vars {}

    array set inar $values
    if {1} {
        foreach x {AR DBGX LDFLAGS LIBS LIB_RUNTIME_DIR VERSION} {
            if {[info exists inar($x)]} {
                set $x $inar($x)
                lappend vars $x [set $x]
            }  else  {
                set $x ""
            }
        }
        foreach n [array names inar] {
            set $n $inar($n)
            lappend vars $n [set $n]
        }
    }

    set fd [open $filename]
    set lines [split [read $fd] \r\n]
    close $fd

    foreach line $lines {
        if {$line == ""} {continue}
        if {$platform == "win32-x86"} {
            # in case we ran into Cygwin
            regsub -all -nocase "/cygdrive/(.)" $line "\\1:" line
        }
        if {[regexp {^(\w+)=$} $line - name]} {
            set value ""
        } elseif {[regexp {^(\w+)='(.*)'$} $line - name value]} {
            set value [string map [list \\ \\\\] $value]
        } elseif {[regexp {^(\w+)="(.*)"$} $line - name value]} {
        } elseif {![regexp {^(\w+)=(.*)$} $line - name value]} {
            continue
        }
        if {[catch {
            set $name [subst $value]
        } error]} {
            foreach {- v} [regexp -all -inline -- "\\\$\\\{(\[A-Za-z_\]+)\\\}" $value] {
                if {![info exists $v]} {
                    set $v ""
                }
            }
            if {[catch {
                set $name [subst $value]
            } error]} {
                log 2 "Error while parsing $filename variable \"$name\": $error"
            }
        }  else  {
            lappend vars $name [set $name]
        }
    }

    return $vars
}

proc cookit::runPartCommand {name command args} {
    set command [linsert $args 0 \
        [list ::cookit::${name}::${command}] \
        ]
    return [uplevel 1 $command]
}

proc cookit::checkPartCommandExists {name command} {
    set command "::cookit::${name}::${command}"
    return [expr {([info commands $command] == "") ? 0 : 1}]
}
