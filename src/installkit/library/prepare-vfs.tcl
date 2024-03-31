# installkit - prepare VFS files
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

set destinationDirectory [lindex $argv 0]
set rootLibDirectory   [file dirname $tcl_library]
set installkitLibDirectory [file dir [info script]]

puts "### Preparing VFS files from $rootLibDirectory to $destinationDirectory directory..."

set filelist [list]

proc isMatchList { str list } {
    foreach mask $list {
        if { [string match -nocase $mask $str] } {
            return 1
        }
    }
    return 0
}

proc genStaticPkgIndex { srcDir package { ver {} } } {
    if { $ver eq "" } {
        puts stderr "Error: version for $src is not specified"
        exit 1
    }
    set script "package ifneeded $package $ver \[list load {} [string totitle $package]\]"
    addFile [file join $srcDir pkgIndex.tcl] $script
}

proc shrink { src dst } {
    set dir [file dirname $dst]
    if { ![file isdirectory $dir] } {
        file mkdir $dir
    }

    if { ![isMatchList [file tail $src] {*.tcl *.tm}] } {
        file copy -force $src $dst
        return
    }

    set fsrc [open $src r]
    fconfigure $fsrc -encoding utf-8 -translation auto
    set fdst [open $dst w]
    fconfigure $fdst -encoding utf-8 -translation lf

    # Line continuations should be deleted before comments are deleted.
    #
    # Let's imagine the following Tcl code:
    #
    #     # some comment here\
    #         another line of comment
    #
    # If we remove everything from '#' to EOL, then the line
    # 'another line of comment' will appear as a code. However, it was originally
    # a comment. Thus, we must first join line continuations and then remove comments.

    # Stage 1. Join line continuations.

    set lines [list]
    unset -nocomplain prev
    while { [gets $fsrc line] != -1 } {
        set line [string trim $line]
        # strip line continuations
        if { [string index $line end] eq "\\" } {
            set line [string range $line 0 end-1]
            if { [info exists prev] } {
                append prev " "
            }
            append prev $line
        } else {
            if { [info exists prev] } {
                set line "$prev $line"
                unset prev
            }
            lappend lines $line
        }
    }
    if { [info exists prev] } {
        lappend lines $line
    }

    # Stage 2. Remove empty lines and comments.

    foreach line $lines {
        if { $line eq "" } continue
        if { [string index $line 0] eq "#" && ![string match "#define*" $line] } continue
        puts $fdst $line
    }

    close $fsrc
    close $fdst
}

proc copyFiles { } {
    puts "* copying selected files..."
    foreach { src script dst } $::filelist {
        if { $script eq "" } {
            # plain files
            shrink $src $dst
        } {
            # scripts
            set dir [file dirname $dst]
            if { ![file isdirectory $dir] } { file mkdir $dir }
            set fdst [open $dst a]
            fconfigure $fdst -encoding utf-8 -translation lf
            puts $fdst $script
            close $fdst
        }
    }
}

proc addFile { src { script {} } { dest - } } {
    if { ![info exists ::rootLibDirParts] } {
        set ::rootLibDirParts [expr { [llength [file split $::rootLibDirectory]] - 1 }]
    }
    if { $dest eq "-" } {
        set dest [file split $src]
        set dest [lrange $dest $::rootLibDirParts end]
        set dest [file join {*}$dest]
    } {
        set dest [file join $dest [file tail $src]]
    }
    puts "- add $src -> ./$dest"
    # Special case for pkgIndex.tcl files. Replace references to static libs.
    if { $script eq "" && [file tail $src] eq "pkgIndex.tcl" } {
        set fp [open $src r]
        fconfigure $fp -encoding utf-8
        set script [read $fp]
        close $fp
        regsub -all {\[file join \$dir \S+\.a\]} $script "{}" script
    }
    lappend ::filelist $src $script [file join $::destinationDirectory $dest]
}

proc addAllFiles { dir { exclude {} } } {
    lappend exclude "*.a" "*.lib" "*[info sharedlibext]"
    foreach file [glob -nocomplain -directory $dir -type f *] {
        if { [isMatchList [file tail $file] $exclude] } continue
        addFile $file
    }
    foreach subdir [glob -nocomplain -directory $dir -type d *] {
        if { [isMatchList [file tail $subdir] $exclude] } continue
        puts "* WRNING: the following directory was ignored: $subdir"
    }
}

proc directoryExists { dir { opt 0 } } {
    if { [file isdirectory $dir] } {
        return 1
    }
    if { $opt } {
        puts "- Directory '$dir' does not exist"
        return 0
    }
    puts stderr "Error: directory '$dir' does not exist"
    exit 1
}

proc findFiles { dir mask } {
    set result [glob -nocomplain -type f -directory $dir $mask]
    foreach subdir [glob -nocomplain -type d -directory $dir *] {
        lappend result {*}[findFiles $subdir $mask]
    }
    return $result
}

proc addTcl { { optional 0 } } {

    puts "* prepare the tcl package:"

    set dirMinor [file join $::rootLibDirectory tcl[info tclversion]]

    if { [directoryExists $dirMinor $optional] } {
        set dir [file join $dirMinor encoding]
        directoryExists $dir
        foreach file [glob -nocomplain -type f -directory $dir *.enc] {
            if { [file size $file] < 4096 } {
                addFile $file
            }
        }
    }

    set dirMajor [file join $::rootLibDirectory tcl[lindex [split [info tclversion] .] 0]]

    if { [directoryExists $dirMajor $optional] } {
        foreach package [findFiles $dirMajor *.tm] {
            if { [isMatchList [file tail $package] {*http-* *msgcat-* *tcltest-*}] } {
                addFile $package
            }
        }
    }

    addAllFiles $dirMinor [list "encoding" "http1.*" "opt*" \
        "tzdata" "msgs" "safe.tcl" "*.c"]

}

proc addTk { { optional 0 } } {

    puts "* prepare the tk package:"

    set dirTtk [file join $::rootLibDirectory tk[info tclversion] ttk]
    addAllFiles $dirTtk

    set dirTk [file join $::rootLibDirectory tk[info tclversion]]
    addAllFiles $dirTk [list "ttk" "images" "msgs" "*.c" "choosedir.tcl" \
        "clrpick.tcl" "dialog.tcl" "fontchooser.tcl" "mkpsenc.tcl" \
        "optMenu.tcl" "safetk.tcl" "tkfbox.tcl" "xmfbox.tcl" "pkgIndex.tcl"]

    if { $::tcl_platform(platform) eq "windows" } {
        genStaticPkgIndex $dirTk Tk [info patchlevel]
    } {
        addFile [file join $dirTk .. libtk[info tclversion][info sharedlibext]]
        addFile [file join $dirTk pkgIndex.tcl] "package ifneeded Tk\
            [info patchlevel]\
            \[list load \[file normalize \[file join \$dir .. libtk[info tclversion][info sharedlibext]\]\]\]"
    }

}

proc addThread { { optional 1 } } {

    puts "* prepare the thread package:"

    set dir [glob -nocomplain -type d -directory $::rootLibDirectory "thread*"]
    if { ![llength $dir] } {
        directoryExists [file join $::rootLibdirectory "thread*"]
        return
    }
    addAllFiles [lindex $dir 0] "pkgIndex.tcl"

    load {} Thread
    genStaticPkgIndex $dir Thread [package present Thread]

}

proc addTwapi { { optional 0 } } {

    puts "* prepare the twapi package:"

    set dir [glob -nocomplain -type d -directory $::rootLibDirectory "twapi*"]
    if { ![llength $dir] } {
        directoryExists [file join $::rootLibdirectory "twapi*"]
        return
    }
    addAllFiles [lindex $dir 0] "LICENSE"

}

proc addVfs { { optional 0 } } {

    puts "* prepare the vfs package:"
    set dir [glob -nocomplain -type d -directory $::rootLibDirectory "vfs*"]
    if { ![llength $dir] } {
        directoryExists [file join $::rootLibdirectory "vfs*"]
        return
    }

    # detect available vfs::zip and vfs::tar versions
    lappend auto_path $::rootLibDirectory
    package require vfs::zip
    package require vfs::tar

    addFile [file join $dir zipvfs.tcl]
    addFile [file join $dir tarvfs.tcl]

    addFile [file join $dir pkgIndex.tcl] "package ifneeded vfs::zip\
        [package present vfs::zip] \[list source \[file join \$dir zipvfs.tcl\]\]"
    addFile [file join $dir pkgIndex.tcl] "package ifneeded vfs::tar\
        [package present vfs::tar] \[list source \[file join \$dir tarvfs.tcl\]\]"

}

proc addInstallkit { { optional 0 } } {

    puts "* prepare the installkit package:"

    set dst "lib/installkit"

    addFile [file join $::installkitLibDirectory jammerTheme.tcl] "" $dst
    addFile [file join $::installkitLibDirectory installkit.tcl] "" $dst
    addFile [file join $::installkitLibDirectory wzipvfs.tcl] "" $dst
    addFile [file join $::installkitLibDirectory installkit-compat.tcl] "" $dst
    addFile [file join $::installkitLibDirectory installkit-stats.tcl] "" $dst

    if { $::tcl_platform(platform) eq "windows" } {

        addFile [file join $::installkitLibDirectory installkit-windows.tcl] "" $dst

        set dir [file join $::rootLibDirectory installkit]
        load {} Registry
        genStaticPkgIndex $dir registry [package present registry]

        addFile [file join $::installkitLibDirectory .. win installkit.ico] "" ""

    }

    addFile "pkgIndex.tcl" "" $dst

}

proc makeManifest { } {
    set rglob [list apply { { dir rglob } {
        set result [glob -nocomplain -directory $dir -type f *]
        foreach dir [glob -nocomplain -directory $dir -type d *] {
            lappend result {*}[{*}$rglob $dir $rglob]
        }
        return $result
    }}]

    set strip [llength [file split $::destinationDirectory]]

    set fh [open [file join $::destinationDirectory manifest.txt] w]
    fconfigure $fh -encoding utf-8 -translation lf

    foreach file [{*}$rglob $::destinationDirectory $rglob] {
        set file [file split $file]
        set file [lrange $file $strip end]
        set file [file join {*}$file]
        puts $fh $file
    }

    close $fh
}

addTcl
addTk
addVfs
if { [::tcl::pkgconfig get threaded] } addThread
addInstallkit

if { $::tcl_platform(platform) eq "windows" } {
    addTwapi
}

addFile "manifest.txt" - ""

copyFiles
makeManifest

puts "### VFS files are ready."