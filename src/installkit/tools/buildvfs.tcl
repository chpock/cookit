## buildvfs.tcl --
##
## Script to build the VFS directory for the installkit.
##

array set versions {
    tcl         {8.6 8.6b1.2}
    tk          {8.6 8.6b1.2}
    thread      2.6.6
    installkit  1.1
}

proc recursive_glob { dir pattern } {
    set files [glob -nocomplain -dir $dir -type f $pattern]
    foreach dir [glob -nocomplain -dir $dir -type d *] {
        eval lappend files [recursive_glob $dir $pattern]
    }
    return $files
}

## Strip all comments and white space from the scripts and
## copy to the specified destination
proc shrink { in out } {
    if {[file extension $in] ne ".tcl"} {
        file copy -force $in $out
        return
    }

    set ifp [open $in r]
    set ofp [open $out w]
    fconfigure $ofp -translation lf

    while {[gets $ifp line] != -1} {
        set line [string trim $line]

        if {[string index $line 0] eq "#" && ![string match "#define*" $line]} {
            continue
        }

        ## Merge split strings back
        if {[string index $line end] eq "\\"} {
            puts -nonewline $ofp [string replace $line end end " "]
        } else {
            puts $ofp $line
        }
    }
    close $ifp
    close $ofp
}

proc dopackage { package inputdir outputdir } {
    global static
    global versions

    file mkdir $outputdir

    set files [recursive_glob $inputdir *]

    set ver [lindex $versions($package) 1]
    if {$ver eq ""} { set ver [lindex $versions($package) 0] }

    set package [string toupper $package 0]

    set map [list $inputdir/ ""]

    set newfiles [list]
    switch -- [string tolower $package] {
        "tcl" {
            set exclude [list "*reg*" "*dde*" "*opt*" "*encod*" \
                "*http1.0*" "*ldAout.tcl" "*safe.tcl" "*tcltest*" \
                "*tclAppInit.c" "*ldAix" "*msgs*" "*tzdata*"]

            set dir [file join $inputdir encoding]
            foreach file [glob -nocomplain -type f -dir $dir *.enc] {
                if {[file size $file] < 4096} { lappend newfiles $file }
            }

            set dir [file join [file dirname $inputdir] tcl8]
            foreach file [recursive_glob $dir *.tm] {
                if {[string match "*http-*" $file]
                    || [string match "*msgcat-*" $file]} {
                    lappend newfiles $file
                    set dirname [file dirname $file]
                    if {$dirname ni $map} { lappend map $dirname/ "" }
                }
            }
        }

        "tk" {
            set dir  [file dirname $inputdir]
            set file [glob -nocomplain -dir $dir libtk*[info sharedlibext]]
            set tail [file tail $file]

            if {!$static} { file copy -force $file $outputdir }

            set exclude [list "*.c" "*images*" "*msgs*" \
                "*choosedir.tcl" "*clrpick.tcl" "*dialog.tcl" \
                "*mkpsenc.tcl" "*optMenu.tcl" "*prolog.ps" "*safetk.tcl" \
                "*tkfbox.tcl" "*xmfbox.tcl"]

            if {$static} {
                set script "installkit::loadTk"
            } else {
                set script "\[list load \[file join \$dir $tail\]\]"
            }
        }

        "thread" {
            if {$static} { set script "installkit::loadThread" }
        }

        "installkit" {
            set dirs [glob -dir $::tclLibDir miniarc*]
            if {[llength $dirs] != 1} {
                return -code error "could not find miniarc"
            }

            set dir [lindex $dirs 0]
            file copy -force [file join $dir miniarc.tcl] $outputdir

            set exclude [list "*boot.tcl" "*main.tcl"]
        }
    }

    lappend exclude "*.a"

    if {$static} { lappend exclude "*[info sharedlibext]" }

    foreach file $files {
        set skip 0
        foreach pattern $exclude {
            if {[string match $pattern $file]} { set skip 1; break }
        }
        if {!$skip} { lappend newfiles $file }
    }

    foreach file $newfiles {
        set filename [string map $map $file]
        set newfile  [file join $outputdir $filename]
        puts "$file -> $filename"

        if {![file exists [file dirname $newfile]]} {
            file mkdir [file dirname $newfile]
        }

        shrink $file $newfile
    }

    if {[info exists script]} {
        set fp [open [file join $outputdir pkgIndex.tcl] w]
        puts $fp "package ifneeded $package $ver $script"
        close $fp
    }
}

proc main { staticOrShared srcDir tclLibDir outputdir } {
    global static
    global versions

    set ::srcDir    $srcDir
    set ::tclLibDir $tclLibDir
    set ::outputdir $outputdir

    set static [string equal $staticOrShared "static"]

    foreach package [array names versions] {
        set ver [lindex $versions($package) 0]
        set dir [file join $tclLibDir $package$ver]
        if {$package eq "installkit"} { set dir [file join $srcDir library] }
        if {[file exists $dir]} {
            dopackage $package $dir [file join $outputdir $package]
        }
    }
}

eval main $argv
