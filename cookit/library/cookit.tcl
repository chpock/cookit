# cookit - main package
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

namespace eval ::cookit {

    # We set the ::cookit::root variable during application initialization
    # in the main.c file. However, the application is initialized only for
    # the main interpreter. Thus, let's set it here to be sure that this
    # variable is available for child/threaded interpreters.
    variable root "//cookit:/"

}

#if { [::tcl::pkgconfig get threaded] } {
#    proc ::cookit::newThread { args } {
#        package require Thread
#        # if the last arg is not parameter for thread::create, but script
#        if { [string index [lindex $args end] 0] ne "-" } {
#            set script [list]
#            lappend script [list set ::parentThread [thread::id]]
#            lappend script [list set ::argv $::argv]
#            lappend script [list set ::argv0 $::argv0]
#            lappend script [list info script $::argv0]
#            lappend script [list set ::tcl_interactive $::tcl_interactive]
#            lappend script [lindex $args end]
#            set args [lreplace $args end end [join $script \n]]
#        }
#        set tid [::thread::create {*}$args]
#        return $tid
#    }
#}

proc ::cookit::recursive_glob { dir pattern } {
    set files [lsort [glob -nocomplain -type f -directory $dir $pattern]]
    foreach dir [lsort [glob -nocomplain -type d -directory $dir *]] {
        lappend files {*}[recursive_glob $dir $pattern]
    }
    return $files
}

proc ::cookit::addfiles { filename arg_files arg_names args } {

    set files [list]
    set names [list]

    foreach file $arg_files name $arg_names {

        lappend files $file
        lappend names $name

        if { ![file isdirectory $file] } {
            continue
        }

        set strip_path [llength [file split $file]]
        foreach path [recursive_glob $file *] {

            lappend files $path

            set path [file split $path]
            set path [lrange $path $strip_path end]
            set path [file join $name {*}$path]

            lappend names $path

        }

    }

    set h [::cookfs::Mount $filename $filename {*}$args]
    set params [list]
    set dirs [list]
    foreach file $files name $names {
        set dir [file dirname $name]
        if { $dir ne "." && [lsearch -exact $dirs $dir] == -1 } {
            lappend dirs $dir
        }
        if { ![file isdirectory $file] } {
            # <destination name> "file" <filename> <size> (the size will
            # be calculated automatically)
            lappend params $name file $file ""
        }
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
    ::cookfs::Unmount $filename
}

proc ::cookit::is_pe_file { exe } {
    set fh [open $exe r]
    fconfigure $fh -translation binary
    set sig [read $fh 2]
    close $fh
    return [expr { $sig eq "MZ" }]
}

proc ::cookit::windows_resources_parse { exe } {

    set fh [open $exe r]
    fconfigure $fh -translation binary

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

proc ::cookit::copy_tcl_runtime { manifest dest } {

    set root [file dirname $manifest]

    set fh [open $manifest r]

    # The 1st pass.
    # Copy *.enc files as one large page. In modern environments, we probably
    # won't use encodings at all, as we probably only need utf8.
    # Also, encoding files are very similar to each other, and keeping them
    # as a single page is very efficient. Practical tests have verified that
    # for lzma compression, 5 is the optimal compression level for these files.
    # Higher compression levels do not reduce the size of the compressed data.
    ::cookfs::Mount $dest $dest \
        -compression lzma:5 \
        -pagesize [expr { 1024 * 1024 * 5 }] \
        -smallfilesize [expr { 1024 * 1024 * 5 }] \
        -smallfilebuffer [expr { 1024 * 1024 * 5 }]

    while { [gets $fh file] != -1 } {
        if { [file extension $file] ne ".enc" } continue
        set dir [file join $dest [file dirname $file]]
        if { ![file isdirectory $dir] } {
            file mkdir $dir
        }
        file copy [file join $root $file] $dir
    }

    ::cookfs::Unmount $dest
    seek $fh 0

    # The 2nd pass.
    # Copy all other files. Use the default compression.
    # -pagesize: Use 1MB as the page size.
    # -smallfilesize: If the file is larger than 512 KB, consider it a non-text
    #                 file and place it on a separate page.
    # -smallfilebuffer: Use a large smallbuffer (5MB) to efficiently sort all
    #                   runtime files before storing them to pages.
    ::cookfs::Mount $dest $dest \
        -pagesize [expr { 1024 * 1024 }] \
        -smallfilesize [expr { 1024 * 512 }] \
        -smallfilebuffer [expr { 1024 * 1024 * 5 }]

    while { [gets $fh file] != -1 } {
        if { [file extension $file] eq ".enc" } continue
        set dir [file join $dest [file dirname $file]]
        if { ![file isdirectory $dir] } {
            file mkdir $dir
        }
        file copy [file join $root $file] $dir
    }

    ::cookfs::Unmount $dest
    close $fh

}

proc ::cookit::makestub { exe } {

    variable root

    set fh [open $exe wb]
    file attributes $::cookit::root -parts [list head $fh]
    close $fh

    copy_tcl_runtime [file join $root manifest.txt] $exe
    set_exec_perms $exe

    return $exe

}

proc ::cookit::ico_file_parse { file } {

    set fh [open $file r]
    fconfigure $fh -translation binary

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

proc ::cookit::windows_resources_update { executable params } {

    set resources [windows_resources_parse $executable]

    set fh [open $executable r+]

    # make sure that we close $fh on any error
    catch {

    if { [dict exists $params icon] && [set iconfile [dict get $params icon]] ne "" } {
        if { ![file readable $iconfile] } {
            return -code error "the icon file does not exist or not readable: $iconfile"
        }
        if { [catch { ico_file_parse $iconfile } icondata] } {
            return -code error "error while parsing the icon file $iconfile: $icondata"
        }
        dict for { id icon } $icondata {
            if { ![dict exists $resources icon $id] } {
                continue
            }
            if { [dict get $resources icon $id size] != [dict get $icon size] } {
                continue
            }
            fconfigure $fh -translation binary
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

    set resource_keys [dict create {*}{
         company          CompanyName
         copyright        LegalCopyright
         fileversion      FileVersion
         productname      ProductName
         productversion   ProductVersion
         filedescription  FileDescription
         originalfilename OriginalFilename
    }]

    if { [dict exists $params versionInfo] } {
        foreach key [dict keys [dict get $params versionInfo]] {
            if { ![dict exists $resource_keys $key] } {
                return -code error "unknown resource: \"$key\""
            }
            set resource_key [dict get $resource_keys $key]
            if { ![dict exists $resources version $resource_key] } {
                return -code error "could not find key $resource_key in versionInfo"
            }
            set val [dict get $params versionInfo $key]
            set res [dict get $resources version $resource_key]
            set maxlen [expr { [dict get $res length] - 1 }]
            set len [string length $val]
            if { $len > $maxlen } {
                return -code error "value size ($len) for key $key\
                    in versionInfo exceeds the maximum length $maxlen"
            }
            append val [string repeat "\000" [expr { $maxlen - $len + 1 }]]

            fconfigure $fh -encoding unicode -translation lf -eofchar {}
            seek $fh [dict get $res offset] start
            puts -nonewline $fh $val

            # change version in fixed version struct
            if { $resource_key eq "FileVersion" } {
                fconfigure $fh -translation binary
                seek $fh [dict get $resources version dwFileVersionMS offset] start
                puts -nonewline $fh [{*}$versionToBytes $val MS]
                seek $fh [dict get $resources version dwFileVersionLS offset] start
                puts -nonewline $fh [{*}$versionToBytes $val LS]
            } elseif { $resource_key eq "ProductVersion" } {
                fconfigure $fh -translation binary
                seek $fh [dict get $resources version dwProductVersionMS offset] start
                puts -nonewline $fh [{*}$versionToBytes $val MS]
                seek $fh [dict get $resources version dwProductVersionLS offset] start
                puts -nonewline $fh [{*}$versionToBytes $val LS]
            }

        }
    }

    set ctime [clock microseconds]
    # Convert unix timestamp to 64-bit Windows FILETIME
    set ctime [expr { $ctime * 10 + 11644473600 }]
    # Convert 64-bit Windows FILETIME to 4 WORD values devided by dot.
    # I.e. it will look like a version number, and we can use
    # [{*}$versionToBytes] to get bytes for writing.
    set ctimeMS  [expr { $ctime >> 32 }]
    set ctimeMSh [expr { $ctimeMS >> 16 }]
    set ctimeMSl [expr { $ctimeMS & 0x0000ffff }]
    set ctimeLS  [expr { $ctime & 0x00000000ffffffff }]
    set ctimeLSh [expr { $ctimeLS >> 16 }]
    set ctimeLSl [expr { $ctimeLS & 0x0000ffff }]
    set ctime "${ctimeMSh}.${ctimeMSl}.${ctimeLSh}.${ctimeLSl}"

    # Update the datetime in fixed version struct
    fconfigure $fh -translation binary
    seek $fh [dict get $resources version dwFileDateMS offset] start
    puts -nonewline $fh [{*}$versionToBytes $ctime MS]
    seek $fh [dict get $resources version dwFileDateLS offset] start
    puts -nonewline $fh [{*}$versionToBytes $ctime LS]

    close $fh

    } res opts

    catch { close $fh }

    return -options $opts $res

}

proc ::cookit::wrap { main_script args } {

    variable mount_options

    set paths_input  [list]
    set paths_output [list]
    set compression  "lzma"
    set output       ""
    set stubfile     ""
    set windows_resources [dict create icon "" versionInfo [dict create]]

    if { $main_script eq "-" } {
        set main_script ""
    }

    if { $main_script ne "" } {
        if { ![file exists $main_script] } {
            return -code error "could not find the main script '$main_script' to wrap"
        }
        lappend paths_input $main_script
        lappend paths_output "main.tcl"
    }

    for { set i 0 } { $i < [llength $args] } { incr i } {

        set arg [lindex $args $i]
        if { [incr i] == [llength $args] } {
            return -code error "missing value for argument '$arg'"
        }
        set val [lindex $args $i]

        switch -exact -- $arg {
            -paths {
                if { [info exists paths] } {
                    foreach path $paths {
                        lappend paths_input $path
                        lappend paths_output [file tail $path]
                    }
                }
                set paths $val
            }
            -path {
                if { [info exists paths] } {
                    foreach path $paths {
                        lappend paths_input $path
                        lappend paths_output [file tail $path]
                    }
                }
                set paths [list $val]
            }
            -to {
                if { ![info exists paths] } {
                    return -code error "no paths were specified for destination path '$val'"
                }
                if { [file pathtype $val] ne "relative" } {
                    return -code error "destination directory must be relative: '$val'"
                }
                foreach path $paths {
                    lappend paths_input $path
                    lappend paths_output [file join $val [file tail $path]]
                }
                unset paths
            }
            -as {
                if { ![info exists paths] } {
                    return -code error "no paths were specified for destination path '$val'"
                }
                if { [llength $paths] != 1 } {
                    return -code error "only one path must be specified before argument -as \"$val\""
                }
                if { [file pathtype $val] ne "relative" } {
                    return -code error "destination path must be relative: '$val'"
                }
                lappend paths_input [lindex $paths 0]
                lappend paths_output $val
                unset paths
            }
            -package {
                lappend paths_input $val
                lappend paths_output [file join "lib" [file tail $val]]
            }
            -compression      { set compression $val }
            -output           { set output      $val }
            -stubfile         { set stubfile    $val }
            -icon             { dict set windows_resources icon $val }
            -company          { dict set windows_resources versionInfo company          $val }
            -copyright        { dict set windows_resources versionInfo copyright        $val }
            -fileversion      { dict set windows_resources versionInfo fileversion      $val }
            -productname      { dict set windows_resources versionInfo productname      $val }
            -productversion   { dict set windows_resources versionInfo productversion   $val }
            -filedescription  { dict set windows_resources versionInfo filedescription  $val }
            -originalfilename { dict set windows_resources versionInfo originalfilename $val }
            default {
                puts "DEFAULT $arg"
                return -code error "unknown argument: '$arg'"
            }
        }

    }

    if { [info exists paths] } {
        foreach path $paths {
            lappend paths_input $path
            lappend paths_output [file tail $path]
        }
        unset paths
    }

    if { $output eq "" } {
        if { $main_script eq "" } {
            return -code error "no output file specified"
        }
        set output [file rootname $main_script]
        if { $::tcl_platform(platform) eq "windows" } {
            append output ".exe"
        }
    }

    if { $stubfile ne "" } {
        file copy -force $stubfile $output
    } else {
        makestub $output
    }

    if { [is_pe_file $output] } {
        windows_resources_update $output $windows_resources
    }

    if { [llength $paths_input] } {
        # Default mount options for other (not Tcl runtime) files:
        # -pagesize: Use 1MB as the page size.
        # -smallfilesize: If the file is larger than 512 KB, treat it as a separate file.
        # -smallfilebuffer: Use a large smallbuffer (64MB) to efficiently sort all
        #                   files before storing them to pages.
        addfiles $output $paths_input $paths_output \
            -compression $compression \
            -pagesize [expr { 1024 * 1024 }] \
            -smallfilesize [expr { 1024 * 512 }] \
            -smallfilebuffer [expr { 1024 * 1024 * 64 }]
    }

    set_exec_perms $output
    return $output

}

proc ::cookit::set_exec_perms { exe } {
    if { $::tcl_platform(platform) eq "windows" } {
        file attributes $exe -readonly 0
    } {
        file attributes $exe -permissions 00755
    }
}
