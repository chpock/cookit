# installkit - main package
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

namespace eval ::installkit {}

if { [::tcl::pkgconfig get threaded] } {
    proc ::installkit::newThread { args } {
        package require Thread
        # if the last arg is not parameter for thread::create, but script
        if { [string index [lindex $args end] 0] ne "-" } {
            set script [list]
            lappend script [list set ::parentThread [thread::id]]
            lappend script [list set ::argv $::argv]
            lappend script [list set ::argv0 $::argv0]
            lappend script [list info script $::argv0]
            lappend script [list set ::tcl_interactive $::tcl_interactive]
            lappend script [list ::installkit::postInit]
            lappend script [lindex $args end]
            set args [lreplace $args end end [join $script \n]]
        }
        set tid [::thread::create {*}$args]
        return $tid
    }
}

proc ::installkit::recursive_glob { dir pattern } {
    set files [lsort [glob -nocomplain -type f -directory $dir $pattern]]
    foreach dir [lsort [glob -nocomplain -type d -directory $dir *]] {
        lappend files {*}[recursive_glob $dir $pattern]
    }
    return $files
}

proc ::installkit::tmpmount {} {
    variable tmpMountCount
    while { [file exists [set mnt "/installkittmpvfs[incr tmpMountCount]"]] } {}
    return $mnt
}

proc ::installkit::addfiles { filename files names } {
    set h [vfs::cookfs::Mount $filename $filename]
    set params [list]
    set dirs [list]
    foreach file $files name $names {
        set dir [file dirname $name]
        if { $dir ne "." && [lsearch -exact $dirs $dir] == -1 } {
            lappend dirs $dir
        }
        # <destination name> "file" <filename> <size> (the size will
        # be calculated automatically)
        lappend params $name file $file ""
    }
    # directories to create
    set dirs2 [list]
    foreach dir $dirs {
        set dir [file join $filename $dir]
        if { ![file isdirectory $dir] } {
            lappend dirs2 $dir
        }
    }
    file mkdir {*}$dirs2
    $h writeFiles {*}$params
    vfs::unmount $filename
}

proc ::installkit::parseWrapArgs { arglist } {

    # Set the default options
    set params [dict create {*}{
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
        versionInfo     {
            CompanyName      {}
            LegalCopyright   {}
            FileVersion      {}
            ProductName      {}
            ProductVersion   {}
            FileDescription  {}
            OriginalFilename {}
        }
    }]

    for { set i 0 } { $i < [llength $arglist] } { incr i } {
        set arg [lindex $arglist $i]
        switch -- $arg {
            "-f" {
                # File which contains files to wrap
                set arg [lindex $arglist [incr i]]
                if { ![file readable $arg] } {
                     return -code error "Could not find list file: $arg.\n\nWrapping aborted."
                }

                set fp [open $arg r]
                while { [gets $fp line] != -1 } {
                    lassign $line file name method
                    dict lappend params wrapFiles $file
                    dict lappend params wrapNames [expr { $name eq "" ? $file : $name }]
                    dict lappend params wrapMethods $method
                }
                close $fp
            }
            "-o" {
                # Output file
                set arg [lindex $arglist [incr i]]
                dict set params executable [string map [list \\ /] $arg]
            }
            "-w" {
                # Input installkit stub file
                set arg [lindex $arglist [incr i]]
                dict set params stubfile [string map [list \\ /] $arg]
            }
            "-method" {
                # Compression method (not supported)
                set method [lindex $arglist [incr i]]
                set method "zlib"; # is not supported
                dict set params method $method
            }
            "-level" {
                # Compression level (not supported)
                dict set params level [lindex $arglist [incr i]]
            }
            "-command" {
                # Progress command
                dict set params command [lindex $arglist [incr i]]
            }
            "-password" {
                # Password (not supported)
                dict set params password [lindex $arglist [incr i]]
            }
            "-icon" {
                # Icon
                dict set params icon [lindex $arglist [incr i]]
            }
            "-company" {
                # Version info: company
                dict set params versionInfo CompanyName [lindex $arglist [incr i]]
            }
            "-copyright" {
                # Version info: copyright
                dict set params versionInfo LegalCopyright [lindex $arglist [incr i]]
            }
            "-fileversion" {
                # Version info: version
                dict set params versionInfo FileVersion [lindex $arglist [incr i]]
            }
            "-productname" {
                # Version info: product
                dict set params versionInfo ProductName [lindex $arglist [incr i]]
            }
            "-productversion" {
                # Version info: product version
                dict set params versionInfo ProductVersion [lindex $arglist [incr i]]
            }
            "-filedescription" {
                # Version info: description
                dict set params versionInfo FileDescription [lindex $arglist [incr i]]
            }
            "-originalfilename" {
                # Version info: original filename
                dict set params versionInfo OriginalFilename [lindex $arglist [incr i]]
            }
            "-catalog" {
                # Catalog
                set arg [lindex $arglist [incr i]]
                if { [lsearch -exact [dict get $params catalogs] $arg] == -1 } {
                    dict lappend params catalogs $arg
                }
            }
            "-package" {
                # A Tcl package to include in our lib/ directory
                set arg [lindex $arglist [incr i]]
                if { [lsearch -exact [dict get $params packages] $arg] == -1 } {
                    dict lappend params packages $arg
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

    # The first argument is out main script
    dict set params mainScript [lindex $arglist $i]
    # All the rest of the arguments are files to be wrapped
    foreach file [lrange $arglist [incr i] end] {
        # It is unclear what directory tree in VFS will have the specified files.
        # However, here we simulate the behaviour of installkit 1.3.0.
        if { [file isdirectory $file] } {
            dict lappend params wrapFiles {*}[recursive_glob $file *]
        } elseif { [file readable $file] } {
            dict lappend params wrapFiles $file
        } {
            return -code error "Not a directory or a readable file: $file"
        }
    }

    return $params

}

proc ::installkit::isPEFile { exe } {
    set fh [open $exe r]
    fconfigure $fh -encoding binary -translation binary
    set sig [read $fh 2]
    close $fh
    return [expr { $sig eq "MZ" }]
}

proc ::installkit::parsePEResources { exe } {

    set fh [open $exe r]
    fconfigure $fh -encoding binary -translation binary

    # make sure that we close $fh on any error
    catch {

    set get32 [list apply { { fh } {
        binary scan [read $fh 4] i r
        return [expr { $r & 0xFFFFFFFF }]
    }} $fh]

    set get16 [list apply { { fh } {
        binary scan [read $fh 2] s r
        return [expr { $r & 0xffff }]
    }} $fh]

    set seek [list apply { { fh offset { pos start } } {
        seek $fh $offset $pos
    }} $fh]

    set skip [list apply { { fh offset } {
        seek $fh $offset current
    }} $fh]

    set read [list apply { { fh num } {
        return [read $fh $num]
    }} $fh]

    set readunicode [list apply { { fh len } {
        set r [read $fh [expr { $len * 2 }]]
        return [encoding convertfrom unicode $r]
    }} $fh]

    # read DOS header
    if { [{*}$read 2] ne "MZ" } {
        return -code error "the exe is not a DOS executable"
    }
    # skip DOS header
    {*}$seek 0x3c
    {*}$seek [{*}$get32]

    # read PE header
    if { [{*}$read 4] ne "PE\000\000" } {
        return -code error "the exe is not a Portable Executable"
    }
    {*}$skip 2;  # Machine
    set sec_num [{*}$get16]
    {*}$skip 12; # TimeDateStamp + PointerToSymbolTable + NumberOfSymbols
    set head_size [{*}$get16]
    {*}$skip 2;  # Characteristics

    if { $head_size == 0 } {
        return -code error "could not find the optional header"
    }

    # read PE optional header
    set magic [{*}$get16]
    if { $magic == 0x20b } {
        set pe "PE32+"
    } elseif { $magic != 0x10b } {
        return -code error "the exe has unsupported format\
            in optional header: [format "0x%x" $magic]"
    } else {
        set pe "PE32"
    }
    # skip PE optional header
    {*}$skip [expr { $head_size - 2 }]

    # read sections table
    for { set i 0 } { $i < $sec_num } { incr i } {
        set sec_name [{*}$read 8]
        # skip unknown section
        if { [string range $sec_name 0 4] ne ".rsrc" } {
            {*}$skip 32
            continue
        }
        {*}$skip 4; # VirtualSize
        set sec_rva [{*}$get32]
        {*}$skip 4; # SizeOfRawData
        set sec_pos [{*}$get32]
        break
    }
    if { ![info exists sec_pos] } {
        return -code error "could not find the resource section"
    }

    set getresdir [list apply {{
        get16 get32 seek skip read readunicode fh
        sec_rva sec_pos dir_pos getresdir { level 0 }
    } {
        incr level
        set save [tell $fh]
        # read the resource directory table
        {*}$seek [expr { $sec_pos + $dir_pos }]
        {*}$skip 12; # Characteristics + Time/Date Stamp + Major Version  + Minor Version
        set num_nm_ent [{*}$get16]
        set num_id_ent [{*}$get16]
        set num_ent [expr { $num_nm_ent + $num_id_ent }]

        set getresname [list apply {{ seek get16 readunicode fh sec_pos dir_pos pos } {
            set save [tell $fh]
            {*}$seek [expr { $sec_pos + ($pos & 0x7fffffff) }]
            set len [{*}$get16]
            set name [{*}$readunicode $len]
            {*}$seek $save
            return $name
        }} $seek $get16 $readunicode $fh $sec_pos $dir_pos]

        set getresdata [list apply {{ seek get32 fh sec_rva sec_pos pos } {
            set save [tell $fh]
            {*}$seek [expr { $sec_pos + $pos }]
            set pos  [{*}$get32]
            set pos  [expr { $pos - $sec_rva + $sec_pos }]
            set size [{*}$get32]
            set cp   [{*}$get32]
            {*}$seek $save
            return [list pos $pos size $size codepage $cp]
        }} $seek $get32 $fh $sec_rva $sec_pos]

        set id2name [dict create {*}{
            1  RT_CURSOR
            2  RT_BITMAP
            3  RT_ICON
            4  RT_MENU
            5  RT_DIALOG
            6  RT_STRING
            7  RT_FONTDIR
            8  RT_FONT
            9  RT_ACCELERATOR
            10 RT_RCDATA
            11 RT_MESSAGETABLE
            12 RT_GROUP_CURSOR
            14 RT_GROUP_ICON
            16 RT_VERSION
            17 RT_DLGINCLUDE
            19 RT_PLUGPLAY
            20 RT_VXD
            21 RT_ANICURSOR
            22 RT_ANIICON
            23 RT_HTML
            24 RT_MANIFEST
        }]

        set resources [list]
        # read the resource entries
        for { set i 0 } { $i < $num_ent } { incr i } {
            set data [{*}$get32]
            if { $data & 0x80000000 } {
                set name [{*}$getresname $data]
            } {
                if { $level == 1 && [dict exists $id2name $data] } {
                    set name [dict get $id2name $data]
                } {
                    set name $data
                }
            }
            set data [{*}$get32]
            if { $data & 0x80000000 } {
                set entry [{*}$getresdir [expr { $data & 0x7fffffff }] $getresdir $level]
            } {
                set entry [{*}$getresdata [expr { $data & 0x7fffffff }]]
            }
            lappend resources $name $entry
        }
        {*}$seek $save

        return $resources
    }} $get16 $get32 $seek $skip $read $readunicode $fh $sec_rva $sec_pos]

    set resources [{*}$getresdir 0 $getresdir]

    if { ![dict exists $resources RT_VERSION] } {
        return -code error "could not find RT_VERSION resource"
    } elseif { ![dict exists $resources RT_VERSION 1] } {
        return -code error "could not find #1 member in RT_VERSION resource"
    } elseif { ![dict exists $resources RT_VERSION 1 1033] } {
        return -code error "could not find 1033 language member in\
            #1 member in RT_VERSION resource"
    }

    # Parse version resource

    set res [dict get $resources RT_VERSION 1 1033]

    set version [dict create]

    # read struct: VS_VERSIONINFO
    {*}$seek [dict get $res pos]
    set ver_len [{*}$get16]
    set val_len [{*}$get16]
    set type   [{*}$get16]
    if { $type != 0 } {
        return -code error "the version resource contains not binary data"
    }
    set sig [{*}$readunicode 15]
    if { $sig ne "VS_VERSION_INFO" } {
        return -code error "failed to check signature in VS_VERSIONINFO struct"
    }
    {*}$skip 4; # padding

    # read struct: VS_FIXEDFILEINFO
    set fix_start [tell $fh]
    set sig [{*}$get32]
    if { $sig != 0xFEEF04BD } {
        return -code error "failed to check signature in VS_FIXEDFILEINFO struct"
    }
    {*}$skip 4; # dwStrucVersion
    dict set version dwFileVersionMS offset [tell $fh]
    dict set version dwFileVersionMS value [{*}$get32]
    dict set version dwFileVersionLS offset [tell $fh]
    dict set version dwFileVersionLS value [{*}$get32]
    dict set version dwProductVersionMS offset [tell $fh]
    dict set version dwProductVersionMS value [{*}$get32]
    dict set version dwProductVersionLS offset [tell $fh]
    dict set version dwProductVersionLS value [{*}$get32]
    {*}$skip 20; # dwFileFlagsMask + dwFileFlags + dwFileOS + dwFileType + dwFileSubtype
    dict set version dwFileDateMS offset [tell $fh]
    dict set version dwFileDateMS value [{*}$get32]
    dict set version dwFileDateLS offset [tell $fh]
    dict set version dwFileDateLS value [{*}$get32]
    # skip VS_FIXEDFILEINFO
    {*}$seek [expr { $fix_start + $val_len }]

    # read struct: StringFileInfo
    set sfi_len [{*}$get16]
    {*}$skip 2; # wValueLength
    set type [{*}$get16]
    if { $type != 1 } {
        return -code error "the StringFileInfo struct contains not text data"
    }
    set sig [{*}$readunicode 14]
    if { $sig ne "StringFileInfo" } {
        return -code error "failed to check signature in StringFileInfo struct"
    }
    {*}$skip 2; # padding

    set st_start [tell $fh]
    # read struct: StringTable
    set st_len [{*}$get16]
    {*}$skip 2; # wValueLength (is always equal to zero)
    set type [{*}$get16]
    if { $type != 1 } {
        return -code error "the StringTable struct contains not text data"
    }
    set locale [{*}$readunicode 8]
    {*}$skip 2; # padding

    set st_end [expr { $st_start + $st_len }]
    while { [tell $fh] < $st_end } {

        # read srtuct: String
        set s_len [{*}$get16]
        set val_len   [{*}$get16]
        set type  [{*}$get16]
        if { $type != 1 } {
            return -code error "the String struct contains not text data"
        }
        set key_len [expr { ($s_len - $val_len * 2 - 6) / 2 }]
        set key [{*}$readunicode $key_len]
        set key [string trimright $key "\000"]
        dict set version $key offset [tell $fh]
        set val [{*}$readunicode $val_len]
        set val [string trimright $val "\000"]
        dict set version $key value $val
        dict set version $key length $val_len
        set key [string map [list " " "."] $key]
        set val [string map [list " " "."] $val]
        {*}$skip [expr { $s_len % 4 }]; # padding

    }

    # Parse icon resource

    set res [dict get $resources RT_ICON]

    set icon [dict create]

    dict for { icon_id res } $res {
        if { ![dict exists $res 1033] } {
            return -code error "could not find 1033 language member in icon#$icon_id resource"
        }
        set res [dict get $res 1033]
        {*}$seek [dict get $res pos]
        {*}$skip 4; # biSize
        set width  [{*}$get32]
        set height [{*}$get32]
        set planes [{*}$get16]
        set bpp    [{*}$get16]
        set id "${width}x${height}@${bpp}"
        # just to ensure that we are really reading an icon
        if { $planes != 1 } {
            return -code error "the number of planes for icon#$icon_id ($id) is not 1"
        }
        dict set icon $id [list \
            offset [dict get $res pos] \
            size   [dict get $res size] \
            width  $width \
            height $height \
            bpp    $bpp \
        ]
    }

    close $fh

    return [dict create version $version icon $icon]

    } res opts

    catch { close $fh }

    return -options $opts $res

}

proc ::installkit::makestub { exe } {

    variable root

    # TODO: optimize and don't read the head into memory
    set pg [::cookfs::c::pages -readonly [info nameofexecutable]]
    set head [$pg gethead]
    set bootstrap [$pg get 0]
    $pg delete

    set fh [open $exe w]
    fconfigure $fh -encoding binary -translation binary
    puts -nonewline $fh $head
    puts -nonewline $fh "IKMAGICIK\0\0\0\0\0\0\0\0"
    close $fh

    set tmpmount [tmpmount]
    mountRoot -bootstrap $bootstrap $exe $tmpmount
    set fh [open [file join $root manifest.txt] r]
    while { [gets $fh file] != -1 } {
        set dir [file join $tmpmount [file dirname $file]]
        if { ![file isdirectory $dir] } {
            file mkdir $dir
        }
        file copy [file join $root $file] [file join $tmpmount $file]
    }
    vfs::unmount $tmpmount

    setExecPerms $exe

    return $exe

}

proc ::installkit::parseIcoFile { file } {

    set fh [open $file r]
    fconfigure $fh -encoding binary -translation binary

    set icons [dict create]

    # make sure that we close $fh on any error
    catch {

    set get32 [list apply { { fh } {
        binary scan [read $fh 4] i r
        return [expr { $r & 0xFFFFFFFF }]
    }} $fh]

    set get16 [list apply { { fh } {
        binary scan [read $fh 2] s r
        return [expr { $r & 0xffff }]
    }} $fh]

    set seek [list apply { { fh offset { pos start } } {
        seek $fh $offset $pos
    }} $fh]

    set skip [list apply { { fh offset } {
        seek $fh $offset current
    }} $fh]

    set read [list apply { { fh num } {
        return [read $fh $num]
    }} $fh]

    set sig [{*}$get16]
    if { $sig != 0 } {
        return -code error "the icon file has incorrect icon format"
    }
    set type [{*}$get16]
    if { $type != 1 } {
        return -code error "the icon file has format $type, but format 1 is expected"
    }
    set num [{*}$get16]

    set data [list]

    for { set i 0 } { $i < $num  } { incr i } {
        {*}$skip 8
        set size [{*}$get32]
        set pos  [{*}$get32]
        lappend data $size $pos
    }

    set i 0
    foreach { size pos } $data {
        {*}$seek $pos
        set sig    [{*}$get32]
        # 0x474E5089 - PNG "\0x89PNG"
        # 0x28 - BMP (biSize field)
        if { $sig != 0x28 } continue
        set width  [{*}$get32]
        set height [{*}$get32]
        set planes [{*}$get16]
        set bpp    [{*}$get16]
        set id "${width}x${height}@${bpp}"
        # just to ensure that we are really reading an icon
        if { $planes != 1 } {
            return -code error "the number of planes for icon#$i ($id) is not 1"
        }
        {*}$seek $pos
        set data [{*}$read $size]
        dict set icons $id [list \
            data   $data \
            size   $size \
            width  $width \
            height $height \
            bpp    $bpp \
        ]
        incr i
    }

    close $fh

    return $icons

    } res opts

    catch { close $fh }

    return -options $opts $res

}

proc ::installkit::updateWindowsResources { params } {

    set resources [parsePEResources [dict get $params executable]]

    set fh [open [dict get $params executable] r+]

    # make sure that we close $fh on any error
    catch {

    set iconfile [dict get $params icon]
    if { $iconfile ne "" } {
        if { ![file readable $iconfile] } {
            return -code error "the icon file does not exist or not readable: $iconfile"
        }
        if { [catch { parseIcoFile $iconfile } icondata] } {
            return -code error "error while parsing the icon file $iconfile: $icondata"
        }
        dict for { id icon } $icondata {
            if { ![dict exists $resources icon $id] } {
                continue
            }
            if { [dict get $resources icon $id size] != [dict get $icon size] } {
                continue
            }
            fconfigure $fh -encoding binary -translation binary
            seek $fh [dict get $resources icon $id offset] start
            puts -nonewline $fh [dict get $icon data]
        }
    }

    set versionToBytes [list apply { { ver type } {
        set ver [split [string trim $ver] .]
        if { $type eq "MS" } {
            lassign [lrange $ver 0 1] h l
        } elseif { $type eq "LS" } {
            lassign [lrange $ver 2 3] h l
        } else {
            return -code error "unknown ver type: $type"
        }
        set h [string trim $h]
        set l [string trim $l]
        if { ![string is integer -strict $h] || $h < 0 || $h > 0xffff } {
            set h 0
        }
        if { ![string is integer -strict $l] || $l < 0 || $l > 0xffff } {
            set l 0
        }
        # use 32bit conversion for 16bit numbers here to avoid signed numbers
        set h [binary format i1 $h]
        set l [binary format i1 $l]
        return [string range $l 0 1][string range $h 0 1]
    }}]

    foreach key [dict keys [dict get $params versionInfo]] {
        if { ![dict exists $resources version $key] } {
            return -error "could not find key $key in versionInfo"
        }
        set val [dict get $params versionInfo $key]
        set res [dict get $resources version $key]
        set maxlen [expr { [dict get $res length] - 1 }]
        set len [string length $val]
        if { $len > $maxlen } {
            return -error "value size ($len) for key $key\
                in versionInfo exceeds the maximum length $maxlen"
        }
        append val [string repeat "\000" [expr { $maxlen - $len + 1 }]]

        fconfigure $fh -encoding unicode -translation lf -eofchar {}
        seek $fh [dict get $res offset] start
        puts -nonewline $fh $val

        # change version in fixed version struct
        if { $key eq "FileVersion" } {
            fconfigure $fh -encoding binary -translation binary
            seek $fh [dict get $resources version dwFileVersionMS offset] start
            puts -nonewline $fh [{*}$versionToBytes $val MS]
            seek $fh [dict get $resources version dwFileVersionLS offset] start
            puts -nonewline $fh [{*}$versionToBytes $val LS]
        } elseif { $key eq "ProductVersion" } {
            fconfigure $fh -encoding binary -translation binary
            seek $fh [dict get $resources version dwProductVersionMS offset] start
            puts -nonewline $fh [{*}$versionToBytes $val MS]
            seek $fh [dict get $resources version dwProductVersionLS offset] start
            puts -nonewline $fh [{*}$versionToBytes $val LS]
        }

    }

    set ctime [clock microseconds]
    # Convert unix timestamp to 64-bit Windows FILETIME
    set ctime [expr { $ctime * 10 + 11644473600 }]
    # Convert 64-bit Windows FILETIME to 4 WORD values devided by dot.
    # I.e. it will look like a version number, and we can use
    # [{*}$versionToBytes] to get bytes to writing.
    set ctimeMS  [expr { $ctime >> 32 }]
    set ctimeMSh [expr { $ctimeMS >> 16 }]
    set ctimeMSl [expr { $ctimeMS & 0x0000ffff }]
    set ctimeLS  [expr { $ctime & 0x00000000ffffffff }]
    set ctimeLSh [expr { $ctimeLS >> 16 }]
    set ctimeLSl [expr { $ctimeLS & 0x0000ffff }]
    set ctime "${ctimeMSh}.${ctimeMSl}.${ctimeLSh}.${ctimeLSl}"

    # Update the datetime in fixed version struct
    fconfigure $fh -encoding binary -translation binary
    seek $fh [dict get $resources version dwFileDateMS offset] start
    puts -nonewline $fh [{*}$versionToBytes $val MS]
    seek $fh [dict get $resources version dwFileDateLS offset] start
    puts -nonewline $fh [{*}$versionToBytes $val LS]

    close $fh

    } res opts

    catch { close $fh }

    return -options $opts $res

}

proc ::installkit::wrap { args } {

    set params [parseWrapArgs $args]

    set mainScript [dict get $params mainScript]
    if { $mainScript ne "" && ![file exists $mainScript] } {
        return -code error "could not find the main script '$mainScript' to wrap"
    }

    set executable [dict get $params executable]
    if { $executable eq "" } {
        if { $mainScript eq "" } {
            return -code error "no output file specified"
        }
        set executable [file rootname $mainScript]
        if { $::tcl_platform(platform) eq "windows" } {
            append executable ".exe"
        }
        dict set params executable $executable
    }

    set stubfile [dict get $params stubfile]
    if { $stubfile ne "" } {
        file copy -force $stubfile $executable
    } else {
        makestub $executable
    }

    if { [isPEFile $executable] } {
        updateWindowsResources $params
    }

    set mnt [::installkit::tmpmount]
    vfs::cookfs::Mount $executable $mnt

    foreach package [dict get $params packages] {
        # Don't try to overwrite packages/libraries.
        set pkg_name [file tail $package]
        if { [file exists [file join $mnt lib $pkg_name]] } {
            continue
        }
        # Copy the package to the VFS
        file copy -force $package [file join $mnt lib]
    }

    foreach catalog [dict get $params catalogs] {
        set dest [file join $mnt catalogs]
        if { ![file isdirectory $dest] } {
            file mkdir $dest
        }
        file copy -force $catalog $dest
    }

    if { $mainScript ne "" } {
        file copy -force $mainScript [file join $mnt main.tcl]
        if { [llength [dict get $params wrapFiles]] } {
            foreach file [dict get $params wrapFiles] name [dict get $params wrapNames] {
                if { [dict get $params command] ne "" } {
                    {*}[dict get $params command] $file
                }
                # This logic, when $name is empty, is strange. But here we simulate
                # the behaviour of installkit v1.3.0.
                if { $name eq "" } {
                    if { [file pathtype $file] eq "absolute" } {
                        # strip volume/root from source filename
                        set name [file join {*}[lrange [file split $file] 1 end]]
                    } else {
                        set name $file
                    }
                }
                set out_name [file join $mnt $name]
                set out_dir  [file dirname $out_name]
                if { ![file isdirectory $out_dir] } {
                    file mkdir -- $out_dir
                }
                file copy -force $file $out_name
            }
        }
    }

    vfs::unmount $mnt
    setExecPerms $executable
    return $executable

}

proc ::installkit::setExecPerms { exe } {
    if { $::tcl_platform(platform) eq "windows" } {
        file attributes $exe -readonly 0
    } {
        file attributes $exe -permissions 00755
    }
}

proc ::installkit::rawStartup { } {
    if { ![info exists ::argv] } return
    set cmd [lindex $::argv 0]
    if { $cmd eq "wrap" } {
        ::installkit::wrap {*}[lrange $::argv 1 end]
        exit 0
    } elseif { $cmd eq "stats" } {
        package require installkit::stats
        ::installkit::stats {*}[lrange $::argv 1 end]
        exit 0
    }
}

if { $::tcl_platform(platform) eq "windows" } {
    package require installkit::Windows
}

package require installkit::compat
