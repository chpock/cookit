# installkit - main package
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

namespace eval ::installkit {}

proc ::installkit::recursive_glob { dir pattern } {
    set files [glob -nocomplain -type f -directory $dir $pattern]
    foreach dir [glob -nocomplain -type d -directory $dir *] {
        lappend files {*}[recursive_glob $dir $pattern]
    }
    return $files
}

proc ::installkit::tmpmount {} {
    variable tmpMountCount
    set prefix "/installkitvfs"
    while { [file exists [set mnt "${prefix}[incr tmpMountCount]"]] } {}
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
        lappend params $name file $file ""
    }
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

proc ::installkit::ParseWrapArgs { arrayName arglist { withFiles 1 } } {
    upvar 1 $arrayName installkit

    # Set the default options
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

    for { set i 0 } { $i < [llength $arglist] } { incr i } {
        set arg [lindex $arglist $i]
        switch -- $arg {
            "-f" {
                # File which contains files to wrap
                set arg [lindex $arglist [incr i]]
                if { !$withFiles } continue
                if { ![file readable $arg] } {
                     return -code error "Could not find list file: $arg.\n\nWrapping aborted."
                }

                set fp [open $arg r]
                while { [gets $fp line] != -1 } {
                    lassign $line file name method
                    lappend installkit(wrapFiles) $file
                    lappend installkit(wrapNames) [expr { $name eq "" ? $file : $name }]
                    lappend installkit(wrapMethods) $method
                }
                close $fp
            }
            "-o" {
                # Output file
                set arg [lindex $arglist [incr i]]
                set installkit(executable) [string map [list \\ /] $arg]
            }
            "-w" {
                # Input installkit stub file
                set arg [lindex $arglist [incr i]]
                set installkit(stubfile) [string map [list \\ /] $arg]
            }
            "-method" {
                # Compression method (not supported)
                set method [lindex $arglist [incr i]]
                set method "zlib"; # is not supported
                set installkit(method) $method
            }
            "-level" {
                # Compression level (not supported)
                set installkit(level) [lindex $arglist [incr i]]
            }
            "-command" {
                # Progress command
                set installkit(command) [lindex $arglist [incr i]]
            }
            "-password" {
                # Password (not supported)
                set installkit(password) [lindex $arglist [incr i]]
            }
            "-icon" {
                # Icon
                set installkit(icon) [lindex $arglist [incr i]]
            }
            "-company" {
                # Version info: company
                lappend installkit(versionInfo) CompanyName [lindex $arglist [incr i]]
            }
            "-copyright" {
                # Version info: copyright
                lappend installkit(versionInfo) LegalCopyright [lindex $arglist [incr i]]
            }
            "-fileversion" {
                # Version info: version
                lappend installkit(versionInfo) FileVersion [lindex $arglist [incr i]]
            }
            "-productname" {
                # Version info: product
                lappend installkit(versionInfo) ProductName [lindex $arglist [incr i]]
            }
            "-productversion" {
                # Version info: product version
                lappend installkit(versionInfo) ProductVersion [lindex $arglist [incr i]]
            }
            "-filedescription" {
                # Version info: description
                lappend installkit(versionInfo) FileDescription [lindex $arglist [incr i]]
            }
            "-originalfilename" {
                # Version info: original filename
                lappend installkit(versionInfo) OriginalFilename [lindex $arglist [incr i]]
            }
            "-catalog" {
                # Catalog
                set arg [lindex $arglist [incr i]]
                if { [lsearch -exact $installkit(catalogs) $arg] == -1 } {
                    lappend installkit(catalogs) $arg
                }
            }
            "-package" {
                # A Tcl package to include in our lib/ directory
                set arg [lindex $arglist [incr i]]
                if { [lsearch -exact $installkit(packages) $arg] == -1 } {
                    lappend installkit(packages) $arg
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

    if { $withFiles } {
        # The first argument is out main script
        set installkit(mainScript) [lindex $arglist $i]
        # All the rest of the arguments are files to be wrapped
        foreach file [lrange $arglist [incr i] end] {
            if { [file isdirectory $file] } {
                lappend installkit(wrapFiles) {*}[recursive_glob $file *]
            } elseif { [file readable $file] } {
                lappend installkit(wrapFiles) $file
            } {
                return -code error "Not a directory or a readable file: $file"
            }
        }
    }

}

proc ::installkit::ParsePEResources { exe } {

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
        return -code error "$exe is not a DOS executable"
    }
    # skip DOS header
    {*}$seek 0x3c
    {*}$seek [{*}$get32]

    # read PE header
    if { [{*}$read 4] ne "PE\000\000" } {
        return -code error "$exe is not a Portable Executable"
    }
    {*}$skip 2;  # Machine
    set sec_num [{*}$get16]
    {*}$skip 12; # TimeDateStamp + PointerToSymbolTable + NumberOfSymbols
    set head_size [{*}$get16]
    {*}$skip 2;  # Characteristics

    if { $head_size == 0 } {
        return -code error "could not find the optional header in $exe"
    }

    # read PE optional header
    set magic [{*}$get16]
    if { $magic == 0x20b } {
        set pe "PE32+"
    } elseif { $magic != 0x10b } {
        return -code error "$exe has unsupported format in optional header: [format "0x%x" $magic]"
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
        return -code error "could not find the resource section in $exe"
    }

    set getresdir [list apply {{ get16 get32 seek skip read readunicode fh sec_rva sec_pos dir_pos getresdir { level 0 } } {
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
        return -code error "could not find RT_VERSION resource in $exe"
    } elseif { ![dict exists $resources RT_VERSION 1] } {
        return -code error "could not find #1 member in RT_VERSION resource in $exe"
    } elseif { ![dict exists $resources RT_VERSION 1 1033] } {
        return -code error "could not find 1033 language member in #1 member in RT_VERSION resource in $exe"
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
        return -code error "the version resource contains not binary data in $exe"
    }
    set sig [{*}$readunicode 15]
    if { $sig ne "VS_VERSION_INFO" } {
        return -code error "failed to check signature in VS_VERSIONINFO struct in $exe"
    }
    {*}$skip 4; # padding

    # read struct: VS_FIXEDFILEINFO
    set fix_start [tell $fh]
    set sig [{*}$get32]
    if { $sig != 0xFEEF04BD } {
        return -code error "failed to check signature in VS_FIXEDFILEINFO struct in $exe"
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
    # skip VS_FIXEDFILEINFO
    {*}$seek [expr { $fix_start + $val_len }]

    # read struct: StringFileInfo
    set sfi_len [{*}$get16]
    {*}$skip 2; # wValueLength
    set type [{*}$get16]
    if { $type != 1 } {
        return -code error "the StringFileInfo struct contains not text data in $exe"
    }
    set sig [{*}$readunicode 14]
    if { $sig ne "StringFileInfo" } {
        return -code error "failed to check signature in StringFileInfo struct in $exe"
    }
    {*}$skip 2; # padding

    set st_start [tell $fh]
    # read struct: StringTable
    set st_len [{*}$get16]
    {*}$skip 2; # wValueLength (is always equal to zero)
    set type [{*}$get16]
    if { $type != 1 } {
        return -code error "the StringTable struct contains not text data in $exe"
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
            return -code error "the String struct contains not text data in $exe"
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
            return -code error "could not find 1033 language member in icon#$icon_id resource in $exe"
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
            return -code error "the number of planes for icon#$icon_id ($id) is not 1 in $exe"
        }
        dict set icon $id [list \
            pos    [dict get $res pos] \
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
