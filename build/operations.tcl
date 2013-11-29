namespace eval cookit {}

set cookit::allOptions(threaded) {Enable threads}
set cookit::allOptions(64bit) {Enable 64-bit support}
set cookit::allOptions(alwaysconfigure) {Always run configure}

proc cookit::buildConfigure {args} {
    variable opt
    variable sourcedirectory
    variable unixwinplatform
    variable unixwinmacosxplatform

    set options {
        {mode.arg               {static}        {Compile statically or dynamically}}
        {sourcepath.arg         {absolute}      {Way to specify source path in: relative, absolute or current}}
        {prefixpath.arg         {absolute}      {Way to specify prefix path in: relative or absolute}}
        {subdirectory.arg       {}              {Run configure from specified subdirectory}}
        {platform                               {Use platform-specific directories}}
        {threads.arg            {0}             {Enable/disable threads}}
        {with-tcl.arg           {}              {Add --with-tcl statement; "", relative or absolute}}
        {with-tk.arg            {}              {Add --with-tk statement; "", relative or absolute}}
        {skipshared                             {Do not pass any information about threads enabled/disabled}}
        {skip64bit                              {Do not pass any information about 64bit enabled/disabled}}
        {skipthreads                            {Do not pass any information about threads enabled/disabled}}
        {always                                 {Configure even if Makefile already exists}}
        {additional.arg         {}              {Additional things to pass to configure script}}
    }
    set usage "?options?"

    if {[catch {
        array set o [cmdline::getoptions args $options $usage]
    } usagetext]} {
        return -code error -errorinfo $usagetext $usagetext
    }

    if {[llength $args] != 0} {
        set args -help
        if {[catch {
            array set o [cmdline::getoptions args $options $usage]
        } usagetext]} {
            return -code error -errorinfo $usagetext $usagetext
        }
    }

    if {[file exists Makefile] && (!$o(always)) && (!$opt(alwaysconfigure))} {
        log 3 "Makefile already exists - skipping configure script"
        return
    }

    set path $sourcedirectory

    if {$o(platform)} {
        if {[file exists [file join $path $unixwinmacosxplatform configure]]} {
            log 5 "Adding platform specific ($unixwinmacosxplatform) directory"
            set path [file join $path $unixwinmacosxplatform]
        }  else  {
            log 5 "Adding platform specific ($unixwinplatform) directory"
            set path [file join $path $unixwinplatform]
        }
    }

    if {$o(sourcepath) == "relative"} {
        log 5 "Converting directory to relative directory"
        set path [wdrelative $path]
    }  elseif {$o(sourcepath) == "current"} {
        set path [pwd]
    }  elseif {$o(sourcepath) == "currentrelative"} {
        set path "."
    }

    set command [list [file join $path $o(subdirectory) configure]]

    if {!$o(skip64bit)} {
        if {$opt(64bit)} {
            lappend command --enable-64bit
        }
    }

    if {!$o(skipthreads)} {
        if {$opt(threaded)} {
            lappend command --enable-threads
        }  else  {
            lappend command --disable-threads
        }
    }

    if {$o(mode) == "static"} {
        set prefix [getInstallStaticDirectory]
        if {!$o(skipshared)} {
            lappend command --disable-shared
        }
    }  else  {
        set prefix [getInstallDynamicDirectory]
    }

    if {$o(prefixpath) == "relative"} {
        log 5 "Adding prefix as relative directory"
        lappend command --prefix=[wdrelative $prefix]
        lappend command --libdir=[wdrelative $prefix]/lib
    }  else  {
        lappend command --prefix=$prefix
        lappend command --libdir=$prefix/lib
    }

    if {$o(with-tcl) != ""} {
        if {$o(mode) == "static"} {
            set tcldir [getBuildStaticDirectory tcl]
        }  else  {
            set tcldir [getBuildDynamicDirectory tcl]
        }
        if {$o(with-tcl) == "relative"} {
            log 5 "Adding path to Tcl as relative directory"
            lappend command "--with-tcl=[wdrelative $tcldir]"
        }  else  {
            lappend command "--with-tcl=$tcldir"
        }
    }
    
    if {$o(with-tk) != ""} {
        if {$o(mode) == "static"} {
            set tkdir [getBuildStaticDirectory tk]
        }  else  {
            set tkdir [getBuildDynamicDirectory tk]
        }
        if {$o(with-tk) == "relative"} {
            log 5 "Adding path to Tk as relative directory"
            lappend command "--with-tk=[wdrelative $tkdir]"
        }  else  {
            lappend command "--with-tk=$tkdir"
        }
    }
    
    set command [concat [lrange $command 0 end] [lrange $o(additional) 0 end]]

    log 4 "Command to run: [list $command]"
    set command [shellScriptCommand $command]

    set eid [startExtCommand $command]
    waitForExtCommand $eid 0
}

proc cookit::buildMake {args} {
    set make [osSpecific "make"]

    set eid [startExtCommand [linsert $make end -v]]
    waitForExtCommand $eid
    cleanupExtCommand $eid

    # TODO: platform-specific make?
    set command [concat $make $args]

    set eid [startExtCommand $command]
    waitForExtCommand $eid 0
}

proc cookit::setEnv {args} {
    set rc {}
    foreach {name value} $args {
        if {[info exists ::env($name)]} {
            # ignore variables that are the same now
            if {[string equal $::env($name) $value]} {
                continue
            }
            lappend rc set $name $::env($name)
        }  else  {
            lappend rc unset $name -
        }
        log 3 "setEnv: Setting environment variable $name=$value"
        set ::env($name) $value
    }
    log 5 "setEnv: Result $rc"
    return $rc
}

proc cookit::revertEnv {rc} {
    foreach {mode name value} $rc {
        switch -- $mode {
            set {
                log 3 "revertEnv: Setting environment variable $name=$value"
                set ::env($name) $value
            }
            unset {
                log 3 "revertEnv: Unsetting environment variable $name"
                catch {unset ::env($name)}
            }
        }
    }
}
