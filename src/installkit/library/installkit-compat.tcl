# installkit - compatibility layer for installkit v1.3
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# InstallJammer 1.3.0 redefines procedures from ::msgcat package. This
# breaks other Tcl code which expects usual ::msgcat::* procedures.
# So, here we define a set of ::msgcat::* procedures which will be
# compatible with InstallJammer and original ::msgcat procedures.

# Load msgcat procedures
package require msgcat
package require vfs

# Define ::msgcat::* procedures for use in InstallJammer

namespace eval ::InstallJammer {}
namespace eval ::InstallJammer::msgcat {
    variable Loclist
}

proc ::InstallJammer::msgcat::mcpreferences { } {
    variable Loclist
    return $Loclist
}

proc ::InstallJammer::msgcat::mc { src args } {

    foreach loc [mcpreferences] {
        if { [info exists ::msgcat::Msgs_${loc}($src)] } { break }
    }

    tailcall mcget $loc $src {*}$args

}

proc ::InstallJammer::msgcat::mcexists { src {locales {}} } {

    if { ![llength $locales] } {
        set locales [mcpreferences]
    }

    foreach locale $locales {
        if { $locale eq "None" } {
            upvar #0 ::info msgs
        } else {
            upvar #0 ::msgcat::Msgs_${locale} msgs
        }
        if { [info exists msgs($src)] } { return 1 }
    }

    return 0

}

proc ::InstallJammer::msgcat::mclocale { args } {

    variable Loclist

    if { [llength $args] == 1 && [lindex $args 0] eq "" } {
        return [set Loclist {}]
    }

    foreach locale $args {
        set loc  {}
        set word ""
        foreach part [split [string tolower $locale] _] {
            set word [string trimleft "${word}_${part}" _]
            if { [set x [lsearch -exact $Loclist $word]] != -1 } {
                set Loclist [lreplace $Loclist $x $x]
            }
            set Loclist [linsert $Loclist 0 $word]
        }
    }

    return [lindex $Loclist 0]

}

proc ::InstallJammer::msgcat::mcset { locale src {dest ""} } {

    if { $locale eq "None" } {
        upvar #0 ::info msgs
    } else {
        upvar #0 ::msgcat::Msgs_${locale} msgs
    }

    if { [llength [info level 0]] == 3 } { set dest $src }

    set msgs($src) $dest

}

proc ::InstallJammer::msgcat::mcunset { locale src } {

    if { $locale eq "None" } {
        upvar #0 ::info msgs
    } else {
        upvar #0 ::msgcat::Msgs_${locale} msgs
    }

    array unset msgs $src

}

proc ::InstallJammer::msgcat::mcmset { locale pairs } {

    if { $locale eq "None" } {
        upvar #0 ::info msgs
    } else {
        upvar #0 ::msgcat::Msgs_${locale} msgs
    }

    array set msgs $pairs

}

proc ::InstallJammer::msgcat::mcgetall { locale } {

    if {$locale eq "None"} {
        upvar #0 ::info msgs
    } else {
        upvar #0 ::msgcat::Msgs_${locale} msgs
    }

    return [array get msgs]

}

proc ::InstallJammer::msgcat::mcget { locale src args } {

    variable renderer

    if {$locale eq "None"} {
        upvar #0 ::info msgs
    } else {
        upvar #0 ::msgcat::Msgs_${locale} msgs
    }

    if { ![info exists renderer($locale)] } {
        set renderer($locale) \
            [expr { [info commands ::${locale}::render] ne "" } ]
    }

    if { [info exists msgs($src)] } {
        set src $msgs($src)

        if { $renderer($locale) } {
            set src [::${locale}::render $src]
        }
    }

    if { [llength $args] } {
        return [uplevel 1 [linsert $args 0 ::format $src]]
    } else {
        return $src
    }

}

proc ::InstallJammer::msgcat::mcclear { locale } {
    unset -nocomplain ::msgcat::Msgs_${locale}
}

proc ::InstallJammer::msgcat::Init { } {
    variable Loclist
    set Loclist $::msgcat::Loclist
}

::InstallJammer::msgcat::Init

# Here we redefine "proc" command to protect ::msgcat procedures.

rename ::proc ::__installkit_proc

::__installkit_proc ::proc { args } {
    # Prevent overriding these procedures
    if { [lindex $args 0] in {
        ::msgcat::mc
        ::msgcat::mcexists
        msgcat::mclocale
        ::msgcat::mcset
        ::msgcat::mcunset
        ::msgcat::mcset
        ::msgcat::mcgetall
        ::msgcat::mcget
        ::msgcat::mcclear
        sha1hex
    } } {
        # if a procedure from the list above exists, return without modifying it
        if { [llength [info command [lindex $args 0]]] } {
            #puts "cought: $args"
            return
        }
    }
    # Let InstallJammer uses msgcat::* procs from own namespace
    # InstallJammer::msgcat::*, but not global ones.
    if { [lindex $args 0] in {
        ::InstallJammer::CommonInit
        ::InstallJammer::GetText
        ::InstallJammer::SubstVar
        ConvertProject
        ::InstallJammer::GetTextData
        ::InstallJammer::AskUserLanguage
        ::InstallAPI::LanguageAPI
        ::InstallAPI::LoadMessageCatalog
        ::InstallAPI::SetVirtualText
        ::InstallAPI::VirtualTextExists
        ::InstallJammer::LoadMessages
        ::InstallJammer::DeleteVirtualText
        ::InstallJammer::EditFinishVirtualText
        ::InstallJammer::LoadVirtualText
    } } {
        set body [lindex $args 2]
        # don't replace anything if the body already contains
        # ::InstallJammer::msgcat procedures
        if { [string first ::InstallJammer::msgcat:: $body] == -1 } {
            # save references to ::msgcat::Msgs_*
            set body [string map "::msgcat::Msgs_ \0\0\0" $body]
            # override references from ::msgcat::* to ::InstallJammer::msgcat::*
            set body [string map {::msgcat:: ::InstallJammer::msgcat::} $body]
            # revert references to ::msgcat::Msgs_*
            set body [string map "\0\0\0 ::msgcat::Msgs_" $body]
            set args [lreplace $args 2 2 $body]
            #puts "replaced [lindex $args 0]: $body"
        }
    }
    tailcall ::__installkit_proc {*}$args
}

# InstallJammer UI fails when ::msgcat for ::tcl::clock is not initialized.
# Let's initialize it by calling 'clock format'
clock format [clock seconds]

# This is a legacy version of ::installkit::parseWrapArgs, which uses arrays.
# This procedure is expected to be available in InstallJammer v1.3.0.
proc ::installkit::ParseWrapArgs { arrayName arglist { withFiles 1 } } {
    upvar 1 $arrayName installkit
    array set installkit [::installkit::parseWrapArgs $arglist]
}

# InstallJammer 1.3.0 expects ::sha1hex procedure to exist and returns
# a hex sha1 hash. This is used to generate UID or to verify passwords.
# Actually, it can be any hash function that returns 16 hex encoded
# bytes (128 bits). We use MD5, which is 128 bits, but add another
# 32 bits (4 bytes) to make it look like sha1, which is 160 bits.
proc ::sha1hex { string } {
    set md5 [::cookfs::md5 $string]
    append md5 [string range $md5 0 7]
    return $md5
}

# InstallJammer 1.3.0 uses crapvfs. Let's forward these requests to cookfs.
namespace eval ::crapvfs {}

proc ::crapvfs::mount { file mountPoint } {
    tailcall ::cookfs::Mount -readonly $file $mountPoint
}

proc ::crapvfs::unmount { mountPoint } {
    tailcall ::cookfs::Unmount $mountPoint
}

# Just ignore this proc
proc ::crapvfs::setPassword {args} {}

# Windows-specific fixes
if { $::tcl_platform(platform) eq "windows" } {

    # There is a high risk that some installer uses $::twapi::builtin_account_sids
    # to check if the current user is an administrator. However, this variable is
    # not available in recent versions of twapi.
    package require twapi
    array set ::twapi::builtin_account_sids {
        administrators  S-1-5-32-544
        users           S-1-5-32-545
        guests          S-1-5-32-546
        "power users"   S-1-5-32-547
    }

}

package provide installkit::compat 1.0.0
