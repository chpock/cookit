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

# InstallJammer 1.3.0 expects ::sha1 procedure to exist and returns
# a binary sha1 hash.
proc ::sha1 { data } {
    tailcall ::cookfs::sha1 -bin $data
}

# InstallJammer 1.3.0 uses crapvfs. Let's forward these requests to cookfs.
namespace eval ::crapvfs {}

proc ::crapvfs::mount { file mountPoint } {
    tailcall ::vfs::cookfs::Mount -readonly $file $mountPoint
}

proc ::crapvfs::unmount { mountPoint } {
    tailcall ::vfs::unmount $mountPoint
}

# Just ignore this proc
proc ::crapvfs::setPassword {args} {}

package provide installkit::compat 1.0.0
