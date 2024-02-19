namespace eval ::installkit {}
namespace eval ::installkit::Windows {}

if {[info exists ::tcl_platform(threaded)]} {
    proc ::installkit::newThread { args } {
        package require Thread

        set body "set ::parentThread [thread::id]
                  source [file join $::installkit::root boot.tcl]
                  set ::argv  [list $::argv]
                  set ::argv0 [list $::argv0]
                  info script [list $::argv0]
                  set ::tcl_interactive [list $::tcl_interactive]"

        if {[string index [lindex $args end] 0] ne "-"} {
            append body \n[lindex $args end]
            set args [lreplace $args end end $body]
        }

        set tid [eval thread::create $args]

        tsv::array set ::installkit::info [array get ::installkit::info]
        thread::send $tid {
            array set ::installkit::info [tsv::array get ::installkit::info]
        }

        return $tid
    }
}

proc ::installkit::recursive_glob { dir pattern } {
    set files [glob -nocomplain -type f -dir $dir $pattern]
    foreach dir [glob -nocomplain -type d -dir $dir *] {
        eval lappend files [::installkit::recursive_glob $dir $pattern]
    }
    return $files
}

proc ::installkit::addfiles { filename files args } {
    set fp [miniarc::open crap $filename a]

    if {[llength $args] == 1} {
        miniarc::addfiles $fp $files [lindex $args 0]
    } else {
        foreach file $files {
            eval [list miniarc::addfile $fp $file] $args
        }
    }

    miniarc::close $fp
}

proc ::installkit::tmpmount {} {
    variable tmpMountCount

    if {![info exists tmpMountCount]} { set tmpMountCount 0 }

    while {1} {
        set mnt /installkitvfs[incr tmpMountCount]
        if {![file exists $mnt]} { break }
    }

    return $mnt
}

proc ::installkit::makestub { args } {
    array set installkit {
        catalogs {}
        packages {}
    }
    ::installkit::ParseWrapArgs installkit $args 0

    if {$installkit(executable) eq ""} {
        return -code error "no output file specified"
    }
    set executable $installkit(executable)

    set nameofexe [info nameofexecutable]
    set stubfile  $nameofexe

    if {$installkit(stubfile) ne ""} {
        set stubfile $installkit(stubfile)
    }

    set stubfile [file normalize $stubfile]
    miniarc::crap::fileinfo $stubfile mntinfo

    set ifp [open $stubfile]
    fconfigure $ifp -translation binary

    set ofp [open $executable w]
    fconfigure $ofp -translation binary

    fcopy $ifp $ofp -size $mntinfo(coreOffset)

    set offset [tell $ofp]

    ## Write out all of the headers for the boot files.
    puts -nonewline $ofp $mntinfo(coreHeaders)

    ## Now we need to output the central header with a null chksum
    ## and the new file offset to our headers.

    seek $ifp $mntinfo(centralOffset) start
    puts -nonewline $ofp [read $ifp 6]

    read $ifp 12
    puts -nonewline $ofp [miniarc::NullPad $offset 12]

    read $ifp 64
    puts -nonewline $ofp [string repeat \0 64]

    set left [expr {$::crapvfs::info(centralHeaderSize) - 82}]
    puts -nonewline $ofp [read $ifp $left]

    close $ifp
    close $ofp

    if {$::tcl_platform(platform) eq "windows"} {
        file attributes $executable -readonly 0
    } else {
        file attributes $executable -permissions 00755
    }

    return $executable
}

proc ::installkit::AddExtras { arrayName fp } {
    upvar 1 $arrayName installkit

    set files [list]
    set names [list]

    foreach package $installkit(packages) {
        set map [list [file dirname $package]/ lib/]
        foreach file [recursive_glob $package *] {
            lappend files $file
            lappend names [string map $map $file]
        }
    }

    foreach file $installkit(catalogs) {
        lappend files $file
        lappend names [file join catalogs [file tail $file]]
    }

    foreach file $files name $names {
        miniarc::addfile $fp $file -name $name -corefile 1
    }
}

## ::installkit::wrap
##
##    Create a single file executable out of the specified scripts and
##    image files.  This is done by appending the specified files to the
##    end of a copy of the installkit program.
##
proc ::installkit::wrap { args } {
    package require miniarc

    ::installkit::ParseWrapArgs installkit $args

    if {[string length $installkit(mainScript)]
        && ![file exists $installkit(mainScript)]} {
        return -code error "Could not find $installkit(mainScript) to wrap."
    }

    if {$installkit(executable) eq ""} {
        if {![string length $installkit(mainScript)]} {
            return -code error "no output file specified"
        }

        set fname [file root $installkit(mainScript)]
        set installkit(executable) $fname
        if {$::tcl_platform(platform) eq "windows"} {
            append installkit(executable) ".exe"
        }

        set args [linsert $args 0 -o $installkit(executable)]
    }

    if {[string length $installkit(stubfile)]} {
        file copy -force $installkit(stubfile) $installkit(executable)

        if {$::tcl_platform(platform) eq "windows"} {
            file attributes $installkit(executable) -readonly 0
        } else {
            file attributes $installkit(executable) -permissions 00755
        }
    } else {
        eval ::installkit::makestub $args
    }

    ::installkit::UpdateWindowsResources $installkit(executable) installkit

    set opts [list]

    if {$installkit(method) eq "zlib"} {
        lappend opts -level $installkit(level)
    } else {
        lappend opts -method $installkit(method)
    }

    if {$installkit(password) ne ""} {
        lappend opts -password $installkit(password)
    }

    set fp [eval miniarc::open crap [list $installkit(executable)] a $opts]

    ::installkit::AddExtras installkit $fp

    ## If no main script was specified we just make a stub.
    if {$installkit(mainScript) ne ""} {
        miniarc::addfile $fp $installkit(mainScript) -corefile 1 -name main.tcl

        if {[llength $installkit(wrapFiles)]} {
            foreach file   $installkit(wrapFiles) \
                    name   $installkit(wrapNames) \
                    method $installkit(wrapMethods) {

                if {$installkit(command) ne ""} { $installkit(command) $file }

                set opts {}
                if {$name ne ""} { lappend opts -name $name }
                if {$method ne ""} { lappend opts -method $method }

                if {![llength $opts]} {
                    miniarc::addfile $fp $file
                } else {
                    eval [list miniarc::addfile $fp $file] $opts
                }
            }
        }
    }

    miniarc::close $fp

    if {$::tcl_platform(platform) eq "windows"} {
        file attributes $installkit(executable) -readonly 0
    } else {
        file attributes $installkit(executable) -permissions 00755
    }

    return $installkit(executable)
}

proc ::installkit::ParseWrapArgs { arrayName arglist {withFiles 1} } {
    upvar 1 $arrayName installkit

    ## Setup some default options.
    array set installkit {
        icon            ""
        level           6
        method          zlib
        command         ""
        catalogs        {}
        packages        {}
        stubfile        ""
        password        ""
        wrapFiles       {}
        wrapNames       {}
        mainScript      ""
        executable      ""
        wrapMethods     {}
        versionInfo     {}
    }

    for {set i 0} {$i < [llength $arglist]} {incr i} {
        set arg  [lindex $arglist $i]
        switch -- $arg {
            "-f" {
                ## They specified a file which contains files to wrap.
                ## We want to read each line of the file as a file to wrap.
                set arg [lindex $arglist [incr i]]
                if {!$withFiles} { continue }
                if {![file readable $arg]} {
                     return -code error \
                         "Could not find list file: $arg.\n\nWrapping aborted."
                }

                set fp [open $arg]
                while {[gets $fp line] != -1} {
                    set file   [lindex $line 0]
                    set name   [lindex $line 1]
                    set method [lindex $line 2]
                    lappend installkit(wrapFiles) $file

                    if {$name eq ""} { set name $file }
                    lappend installkit(wrapNames) $name

                    lappend installkit(wrapMethods) $method
                }
                close $fp
            }

            "-o" {
                ## Output file.
                set arg [lindex $arglist [incr i]]
                set installkit(executable) [string map [list \\ /] $arg]
            }

            "-w" {
                ## Input installkit stub file.
                set arg [lindex $arglist [incr i]]
                set installkit(stubfile) [string map [list \\ /] $arg]
            }

            "-method" {
                ## Compression level.
                set method [lindex $arglist [incr i]]
                if {[catch { package require miniarc::crap::$method }]} {
                    return -code error "invalid method '$method'"
                }

                set installkit(method) $method
            }

            "-level" {
                ## Compression level.
                set installkit(level) [lindex $arglist [incr i]]
            }

            "-command" {
                ## Progress command.
                set installkit(command) [lindex $arglist [incr i]]
            }

            "-password" {
                set installkit(password) [lindex $arglist [incr i]]
            }

            "-icon" {
                set installkit(icon) [lindex $arglist [incr i]]
            }

            "-company" {
                lappend installkit(versionInfo) CompanyName
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-copyright" {
                lappend installkit(versionInfo) LegalCopyright
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-fileversion" {
                lappend installkit(versionInfo) FileVersion
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-productname" {
                lappend installkit(versionInfo) ProductName
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-productversion" {
                lappend installkit(versionInfo) ProductVersion
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-filedescription" {
                lappend installkit(versionInfo) FileDescription
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-originalfilename" {
                lappend installkit(versionInfo) OriginalFilename
                lappend installkit(versionInfo) [lindex $arglist [incr i]]
            }

            "-catalog" {
                set catalog [lindex $arglist [incr i]]
                if {[lsearch -exact $installkit(catalogs) $catalog] < 0} {
                    lappend installkit(catalogs) $catalog
                }
            }

            "-package" {
                ## A Tcl package to include in our lib/ directory.
                set package [lindex $arglist [incr i]]
                if {[lsearch -exact $installkit(packages) $package] < 0} {
                    lappend installkit(packages) $package
                }
            }

            "--" {
                incr i
                break
            }

            default {
                break
            }
        }
    }

    if {$withFiles} {
        ## The first argument after all the optional arguments is
        ## our main script.
        set installkit(mainScript) [lindex $arglist $i]

        ## All the rest of the arguments are files to be wrapped.
        foreach file [lrange $arglist [incr i] end] {
            if {[file isdirectory $file]} {
                eval lappend installkit(wrapFiles) [recursive_glob $file *]
            } elseif {[file readable $file]} {
                lappend installkit(wrapFiles) $file
            }
        }
    }

    return
}

proc ::installkit::UpdateWindowsResources { executable arrayName } {
    upvar 1 $arrayName tmp

    if {[llength $tmp(versionInfo)]} {
        set tmp(versionInfo) [linsert $tmp(versionInfo) 0 \
            OriginalFilename [file tail $executable]]
        eval [list ::installkit::Windows::ResourceInfo $executable] \
            $tmp(versionInfo)
    }

    if {[string length $tmp(icon)]} {
        set mnt [installkit::tmpmount]
        crapvfs::mount $executable $mnt
        set fp [open $mnt/installkit.ico]
        fconfigure $fp -translation binary
        set oldIcon [read $fp]
        close $fp
        crapvfs::unmount $mnt

        installkit::addfiles $executable [list $tmp(icon)] \
            -corefile 1 -name installkit.ico

        set fp [open $tmp(icon)]
        fconfigure $fp -translation binary
        set newIcon [read $fp]
        close $fp

        ::installkit::Windows::ReplaceIcon $executable $oldIcon $newIcon
    }
}

##
## installkit::Windows::ResourceInfo ?file? ?attribute value ...?
##
##      Replace the Windows resource information with new values.
##
proc installkit::Windows::ResourceInfo { file args } {
    if {![llength $args]} { return 1 }

    array set val $args

    set fp [open $file r+]
    fconfigure $fp -translation lf -encoding unicode -eofchar {}
    set data [read $fp [file size $file]]

    set s 0
    if {[info exists val(FileVersion)]
        && [scan $val(FileVersion) "%d.%d.%d.%d" major minor patch build]==4} {
        set s [string first "VS_VERSION_INFO" $data]
        if {$s < 0} {
            close $fp
            return 0
        }
        seek $fp [expr {($s * 2) + 30 + 12}] start

        fconfigure $fp -translation binary
        puts -nonewline $fp [binary format ssss $minor $major $build $patch]
        fconfigure $fp -translation lf -encoding unicode -eofchar {}
    }

    set s [string first "StringFileInfo\000" $data $s]
    if {$s < 0} {
        close $fp
        return 0
    }

    if {![info exists val(CodePage)]} { set val(CodePage) 04b0 }
    if {![info exists val(Language)]} { set val(Language) 0409 }

    incr s -3
    set len [scan [string index $data $s] %c]
    seek $fp [expr {$s * 2}] start

    puts -nonewline $fp [format \
        "%c\000\001StringFileInfo\000%c\000\001%s%s\000" \
        $len [expr {$len - 36}] $val(Language) $val(CodePage)]
    unset val(CodePage) val(Language)

    set olen $len
    set len  [expr {($len / 2) - 30}]
    foreach x [array names val] {
        set vlen [expr {[string length $val($x)] + 1}]
        set nlen [string length $x]
        set npad [expr {$nlen % 2}]
        set tlen [expr {$vlen + $nlen + $npad + 4}]
        set tpad [expr {$tlen % 2}]

        if {($tlen + $tpad) > $len} { set error "too long" ; break }
        puts -nonewline $fp [format "%c%c\001%s\000%s%s\000%s" \
            [expr {$tlen * 2}] $vlen $x [string repeat \000 $npad] \
            $val($x) [string repeat \000 $tpad]]
        set len [expr {$len - $tlen - $tpad}]

    }
    puts -nonewline $fp [string repeat \000 $len]
    puts -nonewline $fp [string range $data [expr {$s + ($olen / 2)}] end]
    close $fp

    if {[info exists error]} { return 0 }
    return 1
}

proc ::installkit::Windows::DecodeIcon {data} {
    set result [list]
    binary scan $data sss - type count
    for {set pos 6} {[incr count -1] >= 0} {incr pos 16} {
        binary scan $data @${pos}ccccssii w h cc - p bc bir io
        if {$cc == 0} { set cc 256 }
        binary scan $data @${io}a$bir image
        lappend result ${w}x${h}/$cc $image
    }
    return $result
}

proc ::installkit::Windows::ReplaceIcon { exeFile oldIconData newIconData } {
    array set new [DecodeIcon $newIconData]

    set fp  [open $exeFile]
    fconfigure $fp -translation binary
    set exe [read $fp [file size $exeFile]]
    close $fp

    set map [list]
    foreach {k v} [DecodeIcon $oldIconData] {
        if {![info exists new($k)]} { continue }
        if {[string length $new($k)] != [string length $v]} { continue }
        lappend map $v $new($k)
    }

    set out [string map $map $exe]
    if {[string length $out] != [string length $exe]} { return 0 }
    if {$out == $exe} { return 0 }

    set fp [open $exeFile w]
    fconfigure $fp -translation binary
    puts -nonewline $fp $out
    close $fp

    return 1
}

proc ::installkit::ExitCleanup { args } {
    if {[info exists ::installkit::cleanupFiles]} {
        foreach file $::installkit::cleanupFiles {
            file delete $file
        }
    }
}

package provide installkit @@KIT_VERSION@@

if {$::tcl_platform(platform) eq "windows"} {
    source [file join [file dirname [info script]] wintcl.tcl]
}
