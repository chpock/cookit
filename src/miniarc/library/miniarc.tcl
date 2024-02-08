namespace eval ::miniarc {
    variable version "0.1"
    variable fileVersion "02"
}

proc miniarc::open { format filename args } {
    if {[catch { package require miniarc::$format }]} {
        set x [miniarc::formats]
        set a [join [lrange $x 0 end-1] ,]
        set b [lindex $x end]
        return -code error "invalid format \"$format\": must be $a or $b"
    }

    set append      0
    set permissions 0644
    if {[llength $args]} {
        ## Check the first argument to see if it's a file mode.
        set idx 0
        set chk [lindex $args $idx]
        if {[string index $chk 0] ne "-"} {
            if {$chk eq "a"} {
                set append 1
            } elseif {$chk eq "w"} {
                set append 0
            } else {
                return -code error "invalid mode \"$chk\": must be a or w"
            }

            ## We found a file mode, now we want to check and
            ## see if the next argument is a permission mask.
            incr idx
            set chk [lindex $args $idx]

            if {[string is integer -strict $chk]} {
                incr idx
                set permissions $chk
            }

            set args [lrange $args $idx end]
        }
    }

    array set _args $args

    set method zlib
    if {[info exists _args(-method)]} {
        set method $_args(-method)
        if {[catch { package require miniarc::${format}::$method }]} {
            return -code error "invalid compression method for specified format"
        }

        unset _args(-method)
    }

    if {[info exists _args(-append)]} {
        set append 1
        unset _args(-append)
    }

    if {[info exists _args(-overwrite)]} {
        set append 0
        unset _args(-overwrite)
    }

    set access {RDWR CREAT BINARY}
    if {!$append} { lappend access TRUNC }

    set fp [::open $filename $access $permissions]
    if {$append} { seek $fp 0 end }

    for {set i 1} {$i} {incr i} {
        set handle miniarc$i
        if {![info exists ::miniarc::$handle:info]} { break }
    }

    upvar #0 ::miniarc::$handle:info info

    set info(fp)      $fp
    set info(file)    [file normalize $filename]
    set info(append)  $append
    set info(format)  $format
    set info(method)  $method
    set info(options) [array get _args]
    set info(changed) 0

    miniarc::${format}::open $handle

    return $handle
}

proc miniarc::close { handle } {
    upvar #0 ::miniarc::$handle:info info

    if {$info(changed)} {
        miniarc::$info(format)::close $handle
    }

    ::close $info(fp)

    unset -nocomplain info
}

proc miniarc::addfile { handle file args } {
    upvar #0 ::miniarc::$handle:info info

    set info(changed) 1

    miniarc::$info(format)::addfile $handle $info(fp) $file \
        [concat $info(options) $args]

    incr info(nitems)
}

proc miniarc::addfiles { handle files args } {
    upvar #0 ::miniarc::$handle:info info

    set info(changed) 1

    set command miniarc::$info(format)::addfile

    if {![llength $args]} {
        foreach file $files {
            $command $handle $info(fp) $file $info(options)
	    incr info(nitems)
        }
    } elseif {[llength $args] == 1} {
        ## One argument is a list of names that corresponds
        ## to the list of files.
        foreach file $files name [lindex $args 0] {
            if {$name eq ""} {
                $command $handle $info(fp) $file $info(options)
            } else {
                $command $handle $info(fp) $file \
                    [concat $info(options) -name [list $name]]
            }
	    incr info(nitems)
        }
    } else {
        array set _args $args
        if {[info exists _args(-progress)]} {
            set progress $_args(-progress)
            unset _args(-progress)
            set args [array get _args]
        }

        ## Multiple arguments are options that need to
        ## be passed to each file being added.
        foreach file $files {
            $command $handle $info(fp) $file [concat $info(options) $args]
	    incr info(nitems)
            if {[info exists progress]} { {*}$progress $file }
        }
    }
}

proc miniarc::addfilelist { handle fileList args } {
    upvar #0 ::miniarc::$handle:info info

    array set _args $args
    if {[info exists _args(-progress)]} {
        set progress $_args(-progress)
        unset _args(-progress)
        set args [array get _args]
    }

    set info(changed) 1

    if {$info(format) eq "crap" && $info(flatheaders)} {
        set len [expr {([llength $fileList] + 2) * 256}]
        puts -nonewline $info(fp) [string repeat \0 $len]
        seek $info(fp) $len start
    }

    foreach list $fileList {
        miniarc::$info(format)::addfile $handle $info(fp) [lindex $list 0] \
            [concat $info(options) $args [lrange $list 1 end]]
        incr info(nitems)
        if {[info exists progress]} { {*}$progress [lindex $list 0] }
    }
}

proc miniarc::formats {} {
    set formats [list]
    foreach package [lsearch -inline -all [package names] miniarc::*] {
        lappend formats [lindex [split [string map {:: :} $package] :] 1]
    }

    return [lsort -unique $formats]
}

proc miniarc::NullPad { string len } {
    return $string[string repeat \0 [expr {$len - [string length $string]}]]
}


## Tar format.

proc miniarc::tar::open { handle } {
    upvar #0 ::miniarc::$handle:info info

    if {$info(append)} {
	## Seek back over the two null blocks at the end.
	seek $info(fp) -1024 end
    }
}

proc miniarc::tar::close { handle } {
    upvar #0 ::miniarc::$handle:info info

    ## Put out two null blocks at the end of the file.
    puts -nonewline $info(fp) [string repeat \0 1024]
}


## Zip format.

proc miniarc::zip::open { handle } {
    upvar #0 ::miniarc::$handle:info info

    set info(nitems)  0
    set info(headers) ""

    if {$info(append)} {
	if {[catch { miniarc::zip::ReadArchiveHeader $info(fp) info } err]} {
	    seek $info(fp) 0 end
	} else {
	    seek $info(fp) -[expr {$info(csize) + 22}] end
            set info(headers) [read $info(fp) $info(csize)]
	    seek $info(fp) -[expr {$info(csize) + 22}] end
	}
    }
}

proc miniarc::zip::close { handle } {
    upvar #0 ::miniarc::$handle:info info

    set pos  [tell $info(fp)]
    set ntoc $info(nitems)

    puts -nonewline $info(fp) $info(headers)

    set len [expr {[tell $info(fp)] - $pos}]

    puts -nonewline $info(fp) [binary format a2c2ssssiis PK {5 6} 0 0 \
                                $ntoc $ntoc $len $pos 0]
}

proc miniarc::zip::u_short { n } {
    return [expr {($n + 0x10000) % 0x10000}]
}

proc miniarc::zip::ReadArchiveHeader { fp arrayName } {
    upvar 1 $arrayName info

    seek $fp -22 end
    set hdr [read $fp 22]

    binary scan $hdr A4ssssiis xhdr ndisk cdisk nitems ntotal csize coff comment 
    if {![string equal "PK\05\06" $xhdr]} {
	return -code error "bad end of central directory record"
    }

    foreach var [list ndisk cdisk nitems ntotal csize coff comment] {
	set info($var) [set $var]
    }

    set info(ndisk)   [u_short $info(ndisk)]
    set info(nitems)  [u_short $info(nitems)]
    set info(ntotal)  [u_short $info(ntotal)]
    set info(comment) [u_short $info(comment)]
}


## Crap format.

proc miniarc::crap::open { handle } {
    upvar #0 ::miniarc::$handle:info info

    set info(offset)  [tell $info(fp)]
    set info(headers) ""

    array set _opts $info(options)
    if {[info exists _opts(-password)]} {
        set info(password) $_opts(-password)
    }

    set info(flatheaders) 0
    if {[info exists _opts(-flatheaders)]} {
        set info(flatheaders) 1
        dict unset info(options) -flatheaders
    }

    if {$info(append)} {
        if {[miniarc::crap::fileinfo -handle $info(fp) crapinfo]} {
            if {$crapinfo(version) == 1} {
                return -code error "cannot read version 1 files"
            }
            ## This is a crap file.  Store all of the headers for
            ## later and then seek back over the central header.
            seek $info(fp) $crapinfo(headerOffset) start
            set info(headers) [read $info(fp) $crapinfo(headerSize)]
            seek $info(fp) $crapinfo(headerOffset) start
            chan truncate $info(fp)
        } else {
            ## This is not a crap file.  We'll assume what is
            ## already there is a file header we're appending to.
            seek $info(fp) 0 end
        }
    }
}

proc miniarc::crap::addfile { handle fp file {options {}} } {
    upvar #0 ::miniarc::$handle:info info

    if {[llength $options]} {
        set method $info(method)
        array set _args $options
        if {[info exists _args(-method)]} { set method $_args(-method) }
        package require miniarc::crap::$method
        miniarc::crap::${method}::addfile $handle $fp $file $options
    } else {
        miniarc::crap::$info(method)::addfile $handle $fp $file
    }
}

proc miniarc::crap::close { handle } {
    upvar #0 ::miniarc::$handle:info info
    upvar #0 ::miniarc::fileVersion version

    set offset [miniarc::NullPad [tell $info(fp)] 12]
    set header "CRAP$version$offset"

    ## Calculate the checksum and output it at the end
    ## of the file along with the offset to the first
    ## file header.
    if {[info commands sha1] ne ""} {
        seek $info(fp) 0 start
        set hunk [expr {1 << 12}]
        set data [read $info(fp) $hunk]
        set sha1 [sha1 $data]
        while {1} {
            set data [read $info(fp) $hunk]
            if {$data eq ""} { break }
            sha1 $data $sha1
        }
        binary scan $sha1 H* hash
        append header [miniarc::NullPad $hash 64]
    } else {
        append header [miniarc::NullPad "" 64]
    }

    if {[info exists info(password)]} {
        binary scan [sha1 $info(password)] H* hash
        append header [miniarc::NullPad $hash 64]
    } else {
        append header [miniarc::NullPad "" 64]
    }

    set header [miniarc::NullPad $header 256]

    if {$info(flatheaders)} {
        seek $info(fp) 0 start
        puts -nonewline $info(fp) $header
    }

    puts -nonewline $info(fp) $info(headers)
    puts -nonewline $info(fp) $header
}

proc miniarc::crap::hash {file args} {
    array set _args $args
    set doProgress [info exists _args(-progress)]

    set fp [::open $file "rb"]

    set size [expr {[file size $file] - 256}]
    set hunk [expr {1 << 12}]
    set data [read $fp $hunk]
    set sha1 [sha1 $data]
    set bytesRead 0
    while {1} {
        if {[expr {$bytesRead + $hunk}] > $size} {
            set hunk [expr {$size - $bytesRead}]
        }
        set data [read $fp $hunk]
        if {$data eq ""} { break }
        sha1 $data $sha1
        incr bytesRead $hunk
        if {$doProgress} {
            {*}$_args(-progress) [expr {($bytesRead * 100) / $size}]
        }
    }
    ::close $fp
    if {$doProgress} { {*}$_args(-progress) 100 }
    binary scan $sha1 H* hash
    return $hash
}

proc miniarc::crap::validate {file args} {
    if {![miniarc::crap::fileinfo $file crapinfo]} {
        return -code error "file \"$file\" is not a valid archive"
    }

    if {$crapinfo(version) == 1} {
        return -code error "cannot read version 1 files"
    }

    set hash [miniarc::crap::hash $file {*}$args]
    return [string equal $hash $crapinfo(checksum)]
}
