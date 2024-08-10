# installkit - writable vfs::zip
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

package require vfs::zip

##### vfs::zip

proc vfs::zip::access { fd name mode } {
    ::vfs::log "access $name $mode"
    if { ![zip::exists $fd $name] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    if { $mode & 2 && ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    return 1
}

proc vfs::zip::fileattributes { fd name args } {
    ::vfs::log "fileattributes $args"
    set attrs [list "-readonly" "-hidden" "-system" "-archive" "-permissions"]
    switch -- [llength $args] {
        0 {
            # list strings
            return $attrs
        }
        1 {
            # get value
            set index [lindex $args 0]
            set attr [string trimleft [lindex $attrs $index] -]
            return [zip::attribute $fd $name $attr]
        }
        2 {
            # set value
            if { ![zip::writable $fd] } {
                vfs::filesystem posixerror $::vfs::posix(EROFS)
            }
            set index [lindex $args 0]
            set attr [string trimleft [lindex $attrs $index] -]
            set val [lindex $args 1]
            return [zip::attribute $fd $name $attr $val]
        }
    }
}

if { ![llength [info commands vfs::zip::Mount_orig]] } {
    rename vfs::zip::Mount vfs::zip::Mount_orig
}

proc vfs::zip::Mount { zipfile local args } {
    set mode ""
    foreach arg $args {
        if { $arg eq "-readwrite" } {
            if { $mode eq "" } {
                set mode "readwrite"
            }
        } elseif { $arg eq "-append" } {
            set mode "append"
        } {
            return -code error "vfs::zip::Mount: unknown option \"$arg\""
        }
    }
    if { ![string length $mode] } {
        tailcall Mount_orig $zipfile $local
    }
    set fd [::zip::open [::file normalize $zipfile] $mode]
    vfs::filesystem mount $local [list ::vfs::zip::handler $fd]
    vfs::RegisterMount $local [list ::vfs::zip::Unmount $fd]
    return $fd
}

proc vfs::zip::utime { fd name actime mtime } {
    ::vfs::log "utime $name $actime $mtime"
    if { ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    if { ![zip::exists $fd $name] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    zip::update_entry $fd $name mtime $mtime
}

proc vfs::zip::deletefile { fd name } {
    ::vfs::log "deletefile $name"
    if { ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    if { ![zip::exists $fd $name] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    zip::stat $fd $name sb
    if { $sb(type) eq "directory" } {
        vfs::filesystem posixerror $::vfs::posix(EISDIR)
    }
    zip::del_entry $fd $name
}

proc vfs::zip::removedirectory { fd name recursive } {
    ::vfs::log "removedirectory $name $recursive"
    if { ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    if { ![zip::exists $fd $name] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    zip::stat $fd $name sb
    if { $sb(type) ne "directory" } {
        vfs::filesystem posixerror $::vfs::posix(ENOTDIR)
    }
    unset sb
    set children [zip::getdir $fd $name *]
    if { [llength $children] } {
        if { !$recursive } {
            vfs::filesystem posixerror $::vfs::posix(ENOTEMPTY)
        }
        foreach child $children {
            set child [file join $name $child]
            zip::stat $fd $child sb
            if { $sb(type) eq "directory" } {
                removedirectory $fd $child 1
            } {
                deletefile $fd $child
            }
            unset sb
        }
    }
    zip::del_entry $fd $name
}

proc vfs::zip::createdirectory { fd name } {
    ::vfs::log "createdirectory $name"
    if { ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    ::zip::add_entry $fd directory $name 0o775
}

if { ![llength [info commands vfs::zip::open_orig]] } {
    rename vfs::zip::open vfs::zip::open_orig
}

proc vfs::zip::open { fd name mode permissions } {

    ::vfs::log "open $name $mode $permissions"
    if { $mode eq "r" || $mode eq "" } {
        tailcall open_orig $fd $name $mode $permissions
    }

    if { ![zip::writable $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EROFS)
    }

    # make sure the parent exists and is a directory
    set parent [file dirname $name]
    if { ![zip::exists $fd $parent] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    zip::stat $fd $parent sb
    if { $sb(type) ne "directory" } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }
    unset sb

    set exists 0
    if { [zip::exists $fd $name] } {
        zip::stat $fd $name sb
        if { $sb(type) eq "directory" } {
            vfs::filesystem posixerror $::vfs::posix(EISDIR)
        }
        if { $sb(type) ne "file" } {
            vfs::filesystem posixerror $::vfs::posix(ENOSYS)
        }
        set exists 1
    } {
        if { $mode eq "r+" } {
            vfs::filesystem posixerror $::vfs::posix(ENOENT)
        }
    }

    if { [zip::locked $fd] } {
        vfs::filesystem posixerror $::vfs::posix(EMFILE)
    }

    set chan [vfs::memchan]
    # larger buffer size speeds up memchan
    fconfigure $chan -translation binary -buffersize 262144

    if { $exists } {
        if { $mode in { r+ a+ a } } {
            set h [open $fd $name r]
            fcopy $h $chan
            close $h

            if { $mode eq "r+" } {
                seek $chan 0 start
            }
        }
        zip::del_entry $fd $name
    }

    zip::add_entry $fd file $name $permissions

    zip::locked $fd true

    return [list $chan [list vfs::zip::on_close $fd $name $chan]]

}

proc vfs::zip::on_close { fd name chan } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc
    zip::update_entry $fd $name channel $chan
    zip::locked $fd false
}

##### zip

proc zip::TimeDos { datetime } {
    set s [clock format $datetime -format {%Y %m %e %k %M %S}]
    scan $s {%d %d %d %d %d %d} year month day hour min sec
    return [expr { (( $year - 1980 ) << 25) | ( $month << 21 ) | ( $day << 16 )
           | ( $hour << 11 ) | ( $min << 5 ) | ( $sec >> 1 ) }]
}

proc zip::write_cb { fd } {
    upvar #0 zip::$fd cb
    set rec [binary format a4ssssiis \
        "PK\05\06"                   \
        $cb(ndisk)                   \
        $cb(cdisk)                   \
        $cb(nitems)                  \
        $cb(nitems)                  \
        $cb(csize)                   \
        $cb(coff)                    \
        [string length $cb(comment)] \
    ]
    append rec $cb(comment)
    puts -nonewline $fd $rec
}

proc zip::write_toc { fd path } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc

    set uname [encoding convertto utf-8 \
        [dict get $toc($path) name]]
    set ucomment [encoding convertto utf-8 \
        [dict get $toc($path) comment]]

    set rec [binary format a4ssssiiiisssssii              \
        "PK\01\02"                                        \
        [dict get $toc($path) vem]                        \
        [dict get $toc($path) ver]                        \
        [dict get $toc($path) flags]                      \
        [dict get $toc($path) method]                     \
        [TimeDos [dict get $toc($path) mtime]]            \
        [dict get $toc($path) crc]                        \
        [dict get $toc($path) csize]                      \
        [dict get $toc($path) size]                       \
        [string length $uname]                            \
        [string length [dict get $toc($path) extra]]      \
        [string length $ucomment]                         \
        0                                                 \
        [dict get $toc($path) attr]                       \
        [expr { ( [dict get $toc($path) mode] << 16 ) | [dict get $toc($path) atx] }] \
        [expr { [dict get $toc($path) ino] - $cb(base) }] \
    ]
    append rec $uname [dict get $toc($path) extra] $ucomment

    puts -nonewline $fd $rec
}

proc zip::write_lfheader { fd path { csize -1 } } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc

    set uname [encoding convertto utf-8 \
        [dict get $toc($path) name]]

    if { $csize != -1 } {
        dict set toc($path) csize $csize
    }

    set lfh [binary format a4sssiiiiss               \
        "PK\03\04"                                   \
        [dict get $toc($path) ver]                   \
        [dict get $toc($path) flags]                 \
        [dict get $toc($path) method]                \
        [TimeDos [dict get $toc($path) mtime]]       \
        [dict get $toc($path) crc]                   \
        [dict get $toc($path) csize]                 \
        [dict get $toc($path) size]                  \
        [string length $uname]                       \
        [string length [dict get $toc($path) extra]] \
    ]
    append lfh $uname [dict get $toc($path) extra]

    if { [dict get $toc($path) ino] == -1 } {
        dict set toc($path) ino [expr { $cb(base) + $cb(coff) }]
        dict set toc($path) lfh [string length $lfh]
        incr cb(coff) [expr { [dict get $toc($path) lfh] + [dict get $toc($path) csize] }]
        incr cb(nitems)
        incr cb(ntotal)
    } elseif { $csize != -1 } {
        incr cb(coff) [dict get $toc($path) csize]
    }

    seek $fd [dict get $toc($path) ino] start
    puts -nonewline $fd $lfh

    updated $fd
}

proc zip::create_toc { args } {
    # vem     unix ($zip::systems(3) -> unix) + version 2.3 (23)
    # flags   [expr { (1<<11) }] - utf-8 comment and path
    # ino     -1                 - will be updated by write_lfheader
    array set toc [list            \
        vem     [expr { ( 3 << 8 ) | 23 }] \
        ver     20                 \
        method  0                  \
        type    file               \
        comment {}                 \
        size    0                  \
        disk    0                  \
        attr    0                  \
        extra   {}                 \
        mtime   [clock seconds]    \
        flags   [expr { (1<<11) }] \
        csize   0                  \
        ino     -1                 \
        crc     0                  \
        name    {}                 \
        depth   0                  \
        mode    0                  \
        atx     0                  \
    ]
    array set toc $args
    if { $toc(type) eq "directory" } {
        set toc(name) "[string trimright $toc(name) /]/"
        # $zip::dosattrs(16) -> directory
        set toc(atx)  16
        set toc(mode) [expr { $toc(mode) | 0x4000 }]
    }
    set depth [llength [file split $toc(name)]]
    return [array get toc]
}

proc zip::locked { fd { state {} } } {
    upvar #0 zip::$fd cb
    if { $state ne "" } {
        if { $state } {
            set cb(locked) 1
        } {
            unset -nocomplain cb(locked)
        }
    }
    return [info exists cb(locked)]
}

proc zip::writable { fd { state {} } } {
    upvar #0 zip::$fd cb
    if { $state ne "" } {
        if { $state } {
            set cb(writable) 1
        } {
            unset -nocomplain cb(writable)
        }
    }
    return [info exists cb(writable)]
}

proc zip::updated { fd } {
    upvar #0 zip::$fd cb
    set cb(updated) 1
}

proc zip::add_entry { fd type name permissions } {
    upvar #0 zip::$fd.toc toc
    upvar #0 zip::$fd.dir cbdir
    set path [string tolower $name]
    set toc($path) [create_toc \
        type $type             \
        mode $permissions      \
        name $name             \
    ]
    write_lfheader $fd $path
    set parent [file dirname $name]
    if { $parent eq "." } { set parent "" }
    lappend cbdir($parent) [file tail $name]
}

proc zip::del_entry { fd name } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc
    upvar #0 zip::$fd.dir cbdir
    set path [string tolower $name]
    if { [dict get $toc($path) ino] != -1 } {
        incr cb(nitems) -1
        incr cb(ntotal) -1
    }
    unset toc($path)
    unset -nocomplain cbdir($path)
    set parent [file dirname $path]
    if { $parent eq "." } { set parent "" }
    set pos [lsearch -exact $cbdir($parent) [file tail $name]]
    if { $pos != -1 } {
        set cbdir($parent) [lreplace $cbdir($parent) $pos $pos]
    }
    updated $fd
}

if { ![llength [info commands zip::EndOfArchive_orig]] } {
    rename zip::EndOfArchive zip::EndOfArchive_orig
}

proc zip::EndOfArchive { fd arr } {
    upvar 1 $arr cb
    zip::EndOfArchive_orig $fd cb
    if { $cb(comment) == 0 } {
        set cb(comment) {}
    } {
        # 22 - size of End of Central Directory Record
        seek $fd [expr { $cb(base) + $cb(csize) + 22 }]
        set cb(comment) [read $fd $cb(comment)]
    }
}

if { ![llength [info commands ::zip::_close_orig]] } {
    rename ::zip::_close ::zip::_close_orig
}

proc zip::_close { fd } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc
    if { [info exists cb(updated)] } {
        set start [expr { $cb(base) + $cb(coff) }]
        seek $fd $start start
        foreach path [array names toc] {
            # skip fake directory records
            if { [dict get $toc($path) ino] == -1 } continue
            write_toc $fd $path
        }
        set end [tell $fd]
        set cb(csize) [expr { $end - $start }]
        write_cb $fd
        chan truncate $fd
    }
    tailcall ::zip::_close_orig $fd
}

proc zip::update_entry { fd name update_type data } {
    upvar #0 zip::$fd cb
    upvar #0 zip::$fd.toc toc

    set path [string tolower $name]

    set mtime [clock seconds]
    if { $update_type eq "mtime" } {
        dict set toc($path) mtime $data
    } elseif { $update_type eq "permissions" } {
        dict set toc($path) mode $data
    } elseif { $update_type ni { channel data file } } {
        return -code error "unknown update type: \"$update_type\""
    } else {

        switch -- $update_type {
            file {
                set mtime [file mtime $data]
                set size [file size $data]
                set chan [open $data r]
            }
            channel {
                set chan $data
                seek $chan 0 end
                set size [tell $chan]
                seek $chan 0 start
            }
            data {
                set size [string length $data]
            }
        }

        dict set toc($path) size $size

        seek $fd [expr {
            $cb(base)
            + [dict get $toc($path) ino]
            + [dict get $toc($path) lfh]
        }] start

        # deflate is the default
        dict set toc($path) method 8
        # Don't try to compress already compressed files smaller
        # than 2 MB.
        if { $size < 0x2000000 || $update_type eq "data" } {
            if { [info exists chan] } {
                set data [read $chan]
            }
            set crc [zlib crc32 $data]
            set cdata [zlib deflate $data]
            set csize [string length $cdata]
            # compression ratio is less than 95%
            if { 1.0 * $csize / $size >= 0.95 } {
                dict set toc($path) method 0
                set cdata $data
                set csize $size
            }
            puts -nonewline $fd $cdata
        } {
            # compress large files regardless of compression ratio
            set crc 0
            set csize 0
            set zstream [zlib stream deflate]
            while { ![eof $chan] } {
                set data [read $chan 4096]
                set crc  [zlib crc32 $data $crc]
                $zstream put $data
                set cdata [$zstream get]
                if { [set len [string length $cdata]] } {
                    puts -nonewline $fd $cdata
                    incr csize $len
                }
            }
            $zstream finalize
            set cdata [$zstream get]
            if { [set len [string length $cdata]] } {
                puts -nonewline $fd $cdata
                incr csize $len
            }
            $zstream close
        }

        if { $update_type eq "file" } {
            close $chan
        }

        dict set toc($path) crc $crc
        write_lfheader $fd $path $csize

    }
    dict set toc($path) mtime $mtime

    updated $fd
}

if { ![llength [info commands ::zip::open_orig]] } {
    rename ::zip::open ::zip::open_orig
}

proc zip::open { path { mode "" } } {
    if { ![string length $mode] } {
        tailcall open_orig $path
    }

    # if mode is readwrite or append

    if { ![file exists $path] || $mode eq "append" } {
        if { [catch {
            set fd [::open $path ab+]
        } err] } {
            catch { close $fd }
            return -code error $fd
        }
        upvar #0 zip::$fd cb
        upvar #0 zip::$fd.toc toc
        upvar #0 zip::$fd.dir cbdir
        array set cb {
            ndisk   0
            cdisk   0
            nitems  0
            ntotal  0
            csize   0
            coff    0
            comment {}
        }
        if { $mode eq "append" } {
            set cb(base) [file size $path]
        } {
            set cb(base) 0
        }
        array set toc {}
        array set cbdir {}
    } {
        set fd [::open $path rb+]
        if { [catch {
            upvar #0 zip::$fd cb
            upvar #0 zip::$fd.toc toc
            upvar #0 zip::$fd.dir cbdir

            zip::EndOfArchive $fd cb

            seek $fd [expr { $cb(base) + $cb(coff) }] start

            array set toc [list]

            for { set i 0 } { $i < $cb(nitems) } { incr i } {
                zip::TOC $fd sb

                set origname [string trimright $sb(name) /]
                set sb(depth) [llength [file split $sb(name)]]

                set name [string tolower $origname]
                set sba [array get sb]
                set toc($name) $sba
                FAKEDIR toc cbdir [file dirname $origname]
            }
            foreach { n v } [array get cbdir] {
                set cbdir($n) [lsort -unique $v]
            }
        } err] } {
            close $fd
            return -code error $err
        }
    }
    writable $fd 1
    return $fd
}

proc zip::attribute { fd name attr args } {

    variable dosattrs
    upvar #0 zip::$fd.toc toc

    set path [string tolower $name]

    if { ![info exists toc($path)] } {
        vfs::filesystem posixerror $::vfs::posix(ENOENT)
    }

    # posix permissions

    if { $attr eq "permissions" } {
        if { [llength $args] } {
            set val [lindex $args 0]
            # Allow to change only user/group/others perms, but keep
            # everything else.
            dict set toc($path) mode [expr {
                ([dict get $toc($path) mode] & 0o177000) |
                ($val & 0o777)
            }]
            updated $fd
        }
        return [format {0o%o} [dict get $toc($path) mode]]
    }

    # dos attributes

    set found 0
    foreach bit [array names dosattrs] {
        if { $dosattrs($bit) eq $attr } {
            set found 1
            break
        }
    }
    if { !$found } {
        return -code error "unknown attribute: \"$attr\""
    }

    if { [llength $args] } {
        set val [lindex $args 0]
        if { $val } {
            dict set toc($path) atx [expr { [dict get $toc($path) atx] | $bit }]
        } else {
            dict set toc($path) atx [expr { [dict get $toc($path) atx] & (0xff ^ $bit) }]
        }
        updated $fd
    }

    return [expr { ([dict get $toc($path) atx] & $bit) != 0 }]

}

if { ![llength [info commands ::zip::exists_orig]] } {
    rename ::zip::exists ::zip::exists_orig
}

proc zip::exists { fd path } {
    # the root directory always exists
    if { $path eq "." } {
        return 1
    }
    tailcall exists_orig $fd $path
}

package provide vfs::wzip 0.1.0
