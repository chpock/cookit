# installkit - Windows support
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

package require registry
package require twapi

namespace eval installkit::Windows {

    variable Wow64FsRedirectionDisabledState 0
    variable Wow64FsRedirectionDisabledPtr

    variable RootKeys

    array set RootKeys {
        HKCR    HKEY_CLASSES_ROOT
        HKCU    HKEY_CURRENT_USER
        HKLM    HKEY_LOCAL_MACHINE
        HKU     HKEY_USERS
        HKCC    HKEY_CURRENT_CONFIG
        HKPD    HKEY_PERFORMANCE_DATA
        HKDD    HKEY_DYN_DATA
    }

}


proc installkit::Windows::WrongNumArgs { command { string "" } } {
    set msg "wrong # args: should be \"installkit::Windows::$command "
    if { $string eq "" } {
        append msg "option ?arg ...?"
    } {
        append msg $string
    }
    append msg "\""
    return $msg
}

proc installkit::Windows::GetKey { key {value ""} } {
    variable RootKeys
    set list [split $key \\]
    if { [info exists RootKeys([lindex $key 0])] } {
        set list [lreplace 0 0 $RootKeys([lindex $key 0])]
    }
    set key [string trim [join $list \\] \\]
    if { [catch { registry get $key $value } result] } {
        set result ""
    }
    return $result
}

proc installkit::Windows::FileExtension { option extension args } {

    set root HKEY_CLASSES_ROOT
    set key "$root\\$extension"

    switch -- $option {
        "exists" {
            return [expr { [GetKey $key] ne "" }]
        }
        "set" {
            if { [llength $args] > 1 } {
                return -code error [WrongNumArgs FileExtension \
                    "set extension ?fileType?"]
            }
            if { [llength $args] } {
                registry set $key "" [lindex $args 0]
            } {
                return [GetKey $key]
            }
        }
        default {
            return -code error "bad option \"$option\": must be exists or set"
        }
    }

}

proc installkit::Windows::FileType { option fileType args } {

    set root HKEY_CLASSES_ROOT
    set key "$root\\$fileType"

    switch -- $option {
        "exists" {
            if { [llength $args] } {
                return -code error [WrongNumArgs FileType \
                    "exists fileType"]
            }
            return [expr { [GetKey $key] ne "" }]
        }
        "set" {
            if { [llength $args] > 1 } {
                return -code error [WrongNumArgs FileType \
                    "set fileType ?title?"]
            }
            if { ![llength $args] } {
                return [GetKey $key]
            }
            registry set $key "" [lindex $args 0]
            return $key
        }
        "icon" {
            if { [llength $args] > 1 } {
                return -code error [WrongNumArgs FileType \
                    "icon fileType ?icon?"]
            }
            append key "\\DefaultIcon"
            if { ![llength $args] } {
                return [GetKey $key]
            }
            registry set $key "" [lindex $args 0]
            return $key
        }
        "command" {
            if { [llength $args] > 2 } {
                return -code error [WrongNumArgs FileType \
                    "command fileType ?command? ?newCommand?"]
            }
            append key "\\Shell"
            # if no ?command? was given, return a list of all the
            # commands for this file type
            if { ![llength $args] } {
                if { [catch { registry keys $key } result] } {
                    return [list]
                }
                return $result
            }
            # append the command to the key
            append key "\\[lindex $args 0]\\command"
            # no ?newCommand? specified
            if { [llength $args] == 1 } {
                return [GetKey $key]
            }
            registry set $key "" [lindex $args 1]
            return $key
        }
        "defaultcommand" {
            if { [llength $args] > 1 } {
                return -code error [WrongNumArgs FileType \
                    "defaultcommand fileType ?defaultcommand?"]
            }
            append key "\\Shell"
            if { ![llength $args] } {
                return [GetKey $key]
            }
            registry set $key "" [lindex $args 0]
            return $key
        }
        "menu" {
            if { ![llength $args] || [llength $args] > 2 } {
                return -code error [WrongNumArgs FileType \
                    "menu fileType command ?title?"]
            }
            append key "\\Shell\\[lindex $args 0]"
            # if ?title? was not specified
            if { [llength $args] < 2 } {
                return [GetKey $key]
            }
            registry set $key "" [lindex $args 1]
            return $key
        }
        "showextension" {
            if { ![llength $args] || [llength $args] > 2 } {
                return -code error [WrongNumArgs FileType \
                    "showextension fileType <always|never> ?0|1?"]
            }
            set type [lindex $args 0]
            if { $type ni { always never } } {
                return -code error "unknown or ambiguous param \"$type\": must be always or never"
            }
            set var "[string totitle $type]ShowExt"
            # if ?1|0? was not specified
            if { [llength $args] < 2 } {
                if { [catch { registry get $key $var }] } {
                    return 0
                } {
                    return 1
                }
            }
            set val [lindex $args 1]
            if { ![string is boolean -strict $val] } {
                return -code error "unknown or ambiguous param \"$val\": must be 0 or 1"
            }
            if { [string is true $val] } {
                registry set $key $var "" sz
            } {
                catch { registry delete $key $var }
            }
            return $val
        }
        default {
            return -code error "unknown or ambiguous option \"$option\":\
                must be exists, set, icon, command, defaultcommand,\
                menu or showextension"
        }
    }

}

proc installkit::Windows::File { option file args } {

    set root HKEY_CLASSES_ROOT
    set ext  [file extension $file]

    switch -- $option {
        "type" {
            if { [llength $args] } {
                return -code error [WrongNumArgs File \
                    "type file"]
            }
            tailcall FileExtension set $ext
        }
        "command" {
            if { [llength $args] != 1 } {
                return -code error [WrongNumArgs File \
                    "command file commandName"]
            }
            set filetype [FileExtension set $ext]
            tailcall FileType command $filetype [lindex $args 0]
        }
        "commands" {
            if { [llength $args] } {
                return -code error [WrongNumArgs File \
                    "commands file"]
            }
            set filetype [FileExtension set $ext]
            tailcall FileType command $filetype
        }
        "defaultcommand" {
            if { [llength $args] } {
                return -code error [WrongNumArgs File \
                    "defaultcommand file"]
            }
            set filetype [FileExtension set $ext]
            tailcall FileType defaultcommand $filetype
        }
        "open" {
            set cmd [File command $file open]
            set file [file nativename $file]
            if { ![regsub {%1} $cmd $file cmd] } {
                append cmd " \"$file\""
            }
            ::twapi::create_process "" -cmdline $cmd -detached 1
        }
        "print" {
            set cmd [File command $file print]
            set file [file nativename $file]
            if { ![regsub {%1} $cmd $file cmd] } {
                append cmd " \"$file\""
            }
            ::twapi::create_process "" -cmdline $cmd -detached 1
        }
        default {
            return -code error "unknown or ambiguous option \"$option\":\
                must be type, command, commands, defaultcommand,\
                open or print"
        }

    }

}

proc installkit::Windows::guid { } {
    tailcall ::twapi::new_guid
}

proc installkit::Windows::drive { option args } {

    switch -- $option {
        "list" {
            if { [llength $args] } {
                return -code error [WrongNumArgs drive \
                    "list"]
            }
            set list [::twapi::find_logical_drives]
            set list [lmap x $list { string trimright $x ":\\" }]
            return $list
        }
        "type" {
            if { [llength $args] != 1 } {
                return -code error [WrongNumArgs drive \
                    "type drive"]
            }
            set type [::twapi::get_drive_type [lindex $args 0]]
            # use type names from installkit 1.3.0
            if { $type eq "invalid" } {
                set type "no root dir"
            }
            return $type
        }
        "size" {
            if { [llength $args] != 1 } {
                return -code error [WrongNumArgs drive \
                    "size drive"]
            }
            set info [::twapi::get_volume_info [lindex $args 0] -size]
            return [dict get $info -size]
        }
        "freespace" {
            if {
                ![llength $args] ||
                [llength $args] > 2 ||
                ([llength $args] == 1 && [lindex $args 0] eq "-user")
            } {
                return -code error [WrongNumArgs drive \
                    "freespace ?-user? drive"]
            }
            set flag "-freespace"
            if { [llength $args] == 2 } {
                if { [lindex $args 0] ne "-user" } {
                    return -code error "unknown flag \"[lindex $args 0]\",\
                        -user is expected"
                }
                set flag "-useravail"
                set drive [lindex $args 1]
            } {
                set drive [lindex $args 0]
            }
            set info [::twapi::get_volume_info $drive $flag]
            return [dict get $info $flag]
        }
        default {
            return -code error "unknown or ambiguous option \"$option\":\
                must be list, type, size or freespace"
        }
    }

}

proc installkit::Windows::trash { args } {

    if { ![llength $args] } {
        return -code error [WrongNumArgs trash \
            "?options? file ?file ...?"]
    }

    # twapi::recycle_files supports only -confirm and -showerror
    # flags. Let's call low level twapi functions directly to
    # support all options from installkit 1.3.0.

    set flags 0x40 ; # FOF_ALLOWUNDO

    for { set i 0 } { $i < [llength $args] } { incr i } {
        set opt [lindex $args $i]
        switch -- $opt {
            "-noconfirmation" {
                set flags [expr { $flags | 0x10  }] ; # FOF_NOCONFIRMATION
            }
            "-noerrorui" {
                set flags [expr { $flags | 0x400 }] ; # FOF_NOERRORUI
            }
            "-silent" {
                set flags [expr { $flags | 0x4   }] ; # FOF_SILENT
            }
            "-simpleprogress" {
                set flags [expr { $flags | 0x100 }] ; # FOF_SIMPLEPROGRESS
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

    # other arguments are files
    set files [lrange $args $i end]

    if { ![llength $files] } {
        return -code error "no files were specified"
    }

    # convert file names to native names
    set files [lmap fn $files {
        file nativename [file join [pwd] [file normalize $fn]]
    }]

    if { [catch {
        ::twapi::Twapi_SHFileOperation 0 3 $files __null__ $flags ""
    } res] } {
        return -code error "failed to move files to recycle bin: $res"
    }

    return [expr { [lindex $res 0] ? false : true }]

}

proc installkit::Windows::getFolder { folder } {
    tailcall ::twapi::get_shell_folder $folder
}

proc installkit::Windows::shellExecute { args } {

    if { [llength $args] < 2 } {
        return -code error [WrongNumArgs shellExecute \
            "?options? command file ?arguments?"]
    }

    set flags [list]
    set wait 0

    for { set i 0 } { $i < [llength $args] } { incr i } {
        set opt [lindex $args $i]
        switch -- $opt {
            "-wait" {
                lappend flags -getprocesshandle 1
                set wait 1
            }
            "-windowstate" {
                set opt [lindex $args [incr i]]
                switch -- $opt {
                    "hidden" {
                        lappend flags -show hide
                    }
                    "maximized" {
                        lappend flags -show maximize
                    }
                    "minimized" {
                        lappend flags -show minimize
                    }
                    "normal" {
                        lappend flags -show normal
                    }
                    default {
                        return -code error "invalid argument \"$opt\" for\
                            -windowstate option: must be hidden,\
                            maximized, minimized or normal"
                    }
                }
            }
            "-workingdirectory" {
                lappend flags -dir [lindex $args [incr i]]
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

    if { $i } {
        set args [lrange $args $i end]
    }

    if { [llength $args] < 2 || [llength $args] > 3 } {
        return -code error [WrongNumArgs shellExecute \
            "?options? command file ?arguments?"]
    }

    set cmd [lindex $args 0]
    if { $cmd ni { default edit explore find open print } } {
        return -code error "invalid command: must be default,\
            edit, explore, find, open or print"
    }
    lappend flags -verb $cmd

    lappend flags -path [lindex $args 1]

    if { [llength $args] > 2 } {
        lappend flags --params [lindex $args 2]
    }

    # exec without waiting
    if { !$wait } {
        tailcall ::twapi::shell_execute {*}$flags
    }

    # wait for the process to complete
    if { [catch {
        ::twapi::shell_execute {*}$flags
    } res opts] } {
        # an error here
        return -options $opts $res
    }

    # process handle is in $res
    ::twapi::wait_on_handle $res

}

proc installkit::Windows::createShortcut { shortcutPath args } {

    if { [file pathtype $shortcutPath] ne "absolute" } {
        return -code error "shortcutPath path type is not absolute:\
            $shortcutPath"
    }

    set flags [list]
    set objexists 0

    for { set i 0 } { $i < [llength $args] } { incr i } {
        set opt [lindex $args $i]
        if { $i == [llength $args] - 1 } {
            return -code error "option \"$opt\" must be followed\
                by an argument"
        }
        set arg [lindex $args [incr i]]
        switch -- $opt {
            "-arguments" {
                lappend flags -args $arg
            }
            "-description" {
                lappend flags -desc $arg
            }
            "-objectpath" {
                lappend flags -path $arg
                set objexists 1
            }
            "-showcommand" {
                if { $arg eq "hidden" } {
                    lappend flags -showcmd 0 ; # SW_HIDE
                } elseif { $arg in { maximized minimized normal } } {
                    lappend flags -showcmd $arg
                } else {
                    return -code error "invalid argument \"$arg\" for \"$opt\"\
                        option: must be hidden, maximized, minimized or normal"
                }
            }
            "-workingdirectory" {
                lappend flags -workdir $arg
            }
            "-icon" {
                if { $i == [llength $args] - 1 } {
                    return -code error "-icon option must have iconPath and iconIndex"
                }
                set idx [lindex $args [incr i]]
                lappend flags -iconpath $arg -iconindex $idx
            }
            default {
                return -code error "invalid option \"$opt\": must be\
                    -arguments, -description, -icon, -objectpath,\
                    -showcommand or -workingdirectory"
            }
        }
    }

    if { !$objexists } {
        return -code error "-objectpath for shortcut \"$shortcutPath\"\
            is not specified"
    }

    tailcall ::twapi::write_shortcut $shortcutPath {*}$flags

}

proc installkit::Windows::disableWow64FsRedirection { } {

    variable Wow64FsRedirectionDisabledState
    variable Wow64FsRedirectionDisabledPointer

    if { !$Wow64FsRedirectionDisabledState } {
        if { [catch {
            set Wow64FsRedirectionDisabledPointer [::twapi::Wow64DisableWow64FsRedirection]
        } res opts] } {
            return 0
        }
        set Wow64FsRedirectionDisabledState 1
    }

    return 1

}

proc installkit::Windows::revertWow64FsRedirection { } {

    variable Wow64FsRedirectionDisabledState
    variable Wow64FsRedirectionDisabledPointer

    if { !$Wow64FsRedirectionDisabledState } {
        return 0
    }

    if { [catch {
        ::twapi::Wow64RevertWow64FsRedirection $Wow64FsRedirectionDisabledPointer
    } res opts] } {
        puts "Error: $res and $opts"
        return 0
    }
    set Wow64FsRedirectionDisabledState 0
    return 1

}


package provide installkit::Windows 1.1.1
