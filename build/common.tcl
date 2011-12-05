namespace eval cookit {}

set cookit::allOptions(vfsrepack-interp.arg) {{} {Tcl interpreter command to use for repacking dynamically created packages; defaults to cookit built for this build variant}}

set cookit::allOptions(vfsrepack-compression.arg) {{zlib} {Compression to use for VFS repack}}

proc cookit::buildVersionString {version count} {
    set version [split $version ".ab"]
    return [join [lrange $version 0 [expr {$count - 1}]] .]
}

proc cookit::mkunique {list args} {
    set rc [list]
    foreach e $list {
        set match 0
        foreach a $args {
            if {[string match $a $e]} {set match 1; break}
        }
        if {$match} {
            if {![info exists done($e)]} {
                set done($e) 1
                lappend rc $e
            }
        }  else  {
            lappend rc $e
        }
    }
    return $rc
}

proc cookit::optsort {list args} {
    array set el {}
    foreach e $list {
        set idx 1; set rev 0
        set eidx 0; set erev 0
        foreach a $args {
            if {$a=="-"} {set rev 1; continue}
            if {[string match $a $e]} {
                set eidx $idx
                set erev $rev
                if {![info exists el($eidx)]} {
                    set el($eidx) [list]
                }
            }
            set rev 0
            incr idx
        }
        if {$erev} {
            set el($eidx) [linsert $el($eidx) 0 $e]
        }  else  {
            lappend el($eidx) $e
        }
    }
    set rc [list]
    foreach i [lsort -integer [array names el]] {
        set rc [concat $rc $el($i)]
    }
    return $rc
}

proc cookit::getDownloadsDirectory {} {
    variable rootdirectory
    return [file join $rootdirectory downloads]
}

proc cookit::getOutputDirectory {} {
    variable outputdirectory
    variable platform
    return $outputdirectory
}

proc cookit::getToolsDirectory {} {
    variable toolsdirectory
    variable platform
    return $toolsdirectory
}

proc cookit::getOutputPackagesDirectory {} {
    return [file join [getOutputDirectory] packages]
}

proc cookit::getBuildDirectory {name} {
    variable partbuildmode
    if {$partbuildmode($name) == "static"} {
        return [getBuildStaticDirectory $name]
    }  else  {
        return [getBuildDynamicDirectory $name]
    }
}

proc cookit::getBuildStaticDirectory {name} {
    variable rootdirectory
    variable hostbuilddirectory
    return [file join $rootdirectory _build_static $hostbuilddirectory $name]
}

proc cookit::getBuildDynamicDirectory {name} {
    variable rootdirectory
    variable hostbuilddirectory
    return [file join $rootdirectory _build_dynamic $hostbuilddirectory $name]
}

proc cookit::getSourceDirectory {name {version ""}} {
    variable rootdirectory
    if {$version == ""} {
        set version [getPartVersion $name]
    }
    if {[checkPartCommandExists $name getSourceDirectory]} {
        set rc [runPartCommand $name getSourceDirectory $version]
    }  else  {
        set rc [file join $rootdirectory sources $name-$version]
    }
    return $rc
}

proc cookit::getInstallStaticDirectory {} {
    variable installstaticdirectory
    return $installstaticdirectory
}

proc cookit::getInstallDynamicDirectory {} {
    variable installdynamicdirectory
    return $installdynamicdirectory
}

proc cookit::mkrelative {from to} {
    set fromidx 0
    set rc ""
    while {[set pfrom [file dirname $from]]!=$from} {
        set toidx 0
        set tto $to
        set ttt ""
        while {[set pto [file dirname $tto]]!=$tto} {
            if {$tto==$from} {
                set rc ""
                for {set i 0} {$i<$fromidx} {incr i} {
                    set rc [file join $rc ..]
                }
                set rc [file join $rc $ttt]
                return $rc
            }
            incr toidx
            set ttt [file join [file tail $tto] $ttt]
            set tto $pto
        }
        incr fromidx
        set from $pfrom
    }
    return $to
}

proc cookit::wdrelative {to} {
    set rc [mkrelative [pwd] $to]
    if {$rc==""} {return "."}
    return $rc
}

proc cookit::filterFilelist {filelist args} {
    set rc [list]
    foreach {path a b c} $filelist {
        set matchinclude 0
        set matchexclude 0
        set numinclude 0
        set numexclude 0
        foreach {filter type value} $args {
            switch -- $type {
                regexp {
                    set match [regexp $value $path]
                }
                match {
                    set match [string match $value $path]
                }
                default {
                    error "Invalid filelist match type: $type"
                }
            }
            switch -- $filter {
                include - exclude {
                    incr num$filter
                    if {$match} {
                        set match$filter 1
                    }
                }
                default {
                    error "Invalid filelist filter: $filter"
                }
            }
        }

        if {$matchexclude} {continue}
        if {($numinclude > 0) && (!$matchinclude)} {continue}
        
        lappend rc $path $a $b $c
    }
    return $rc
}

proc cookit::listAllFiles {path directory {level 100}} {
    set rc [list]
    incr level -1
    if {$level == 0} {return [list]}
    foreach g [lsort [glob -type d -nocomplain -directory $directory *]] {
        set gt [file tail $g]
        set rc [concat $rc [listAllFiles [file join $path $gt] $g $level]]
    }
    foreach g [lsort [glob -type f -nocomplain -directory $directory *]] {
        set gt [file tail $g]
        lappend rc [file join $path $gt] file $g ""
    }
    return $rc
}

proc cookit::writeFilelist {destination list} {
    foreach {filename type data size} $list {
        set dest [file join $destination $filename]
        catch {file mkdir [file dirname $dest]}

        switch -- $type {
            data {
                set fh [open $dest w]
                fconfigure $fh -translation binary
                puts -nonewline $fh $dest
                close $fh
            }
            file {
                file copy -force $data $dest
            }
            default {
                error "Unsupported file type: $type"
            }
        }
    }
}

proc cookit::regexpFileLine {filename regexp args} {
    if {[file exists $filename]} {
        set fh [open $filename]
        set fc [read $fh]
        close $fh
        set cmd [list regexp -line -- $regexp $fc]
        set cmd [concat $cmd $args]
        return [uplevel 1 $cmd]
    }
    return 0
}

proc cookit::getCookitName {{relative 1}} {
    variable platform
    variable opt

    set output "cookit"

    if {$platform == "win32-x86"} {
        set outputsuffix ".exe"
    }  else  {
        set outputsuffix ""
    }

    if {$opt(binarysuffix) != ""} {
        set outputsuffix "-$opt(binarysuffix)$outputsuffix"
    }

    log 5 "Cookit base filename \"$output\"; suffix: \"$outputsuffix\""

    set output "$output$outputsuffix"

    log 3 "Cookit filename: $output"

    if {$relative} {
        set dirname [wdrelative [getOutputDirectory]]
    }  else  {
        set dirname [getOutputDirectory]
    }

    set output [file join $dirname $output]

    log 3 "Cookit file path: $output"

    return $output
}

proc cookit::cfsRepack {args} {
    variable opt
    variable rootdirectory

    if {$opt(vfsrepack-interp) != ""} {
        set cmd [list $opt(vfsrepack-interp)]
    }  else  {
        set cmd [list [getCookitName]]
    }

    lappend cmd [wdrelative [file join $rootdirectory build scripts cfsrepack.tcl]]
    lappend cmd compression $opt(vfsrepack-compression)
    set cmd [concat $cmd $args]

    set eid [startExtCommand $cmd]
    waitForExtCommand $eid 0
}

proc cookit::addToPath {binpath libpath command} {
    variable platform
    set revert [list]
    if {$platform == "win32-x86"} {
        catch {lappend revert ::env(PATH) $::env(PATH)}
        set ::env(PATH) "$::env(PATH);[join [concat $binpath $libpath] \;]"
    }  else  {
        catch {lappend revert ::env(PATH) $::env(PATH)}
        catch {lappend revert ::env(LD_LIBRARY_PATH) $::env(LD_LIBRARY_PATH)}
        set oldlibpath ""
        catch {set oldlibpath $::env(LD_LIBRARY_PATH)}
        set ::env(PATH) "$::env(PATH):[join $binpath :]"
        set ::env(LD_LIBRARY_PATH) "${oldlibpath}:[join $libpath :]"
        set pathseparator ":"
    }

    if {[catch {
        uplevel 1 $command
    } e]} {
        set ei $::errorInfo
        set ec $::errorCode
        foreach {var val} $revert {set $var $val}
        return -code error -errorinfo $ei -errorcode $ec $e
    }
    foreach {var val} $revert {set $var $val}
}
