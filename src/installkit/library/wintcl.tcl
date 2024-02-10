## If we don't have the registry package, none of these commands will work.
if {[catch { package require registry }]} { return }

namespace eval installkit::Windows {
    variable  RootKeys
    array set RootKeys {
        HKCR    HKEY_CLASSES_ROOT
        HKCU    HKEY_CURRENT_USER
        HKLM    HKEY_LOCAL_MACHINE
        HKU     HKEY_USERS
        HKCC    HKEY_CURRENT_CONFIG
    }
}

proc installkit::Windows::WrongNumArgs { command {string ""} } {
    append msg {wrong # args: should be "} "installkit::Windows::$command "
    if {![string length $string]} {
        append msg {option ?arg ...?"}
    } else {
        append msg $string
    }
    return $msg
}

proc installkit::Windows::GetKey { key {value ""} } {
    variable RootKeys

    set list [split $key \\]
    if {[info exists RootKeys([lindex $list 0])]} {
        set list [lreplace $list 0 0 $RootKeys([lindex $list 0])]
    }
    set key [string trim [join $list \\] \\]

    if {![catch {registry get $key $value} result]} {
        return $result
    }
}

proc installkit::Windows::FileExtension { option extension args } {
    set root HKEY_CLASSES_ROOT

    switch -- $option {
        "exists"        {
            set key "$root\\$extension"
            return [expr [string length [GetKey $key]] > 0]
        }

        "set"           {
            if {[llength $args] > 1} {
                set msg [WrongNumArgs FileExtension "set extension ?fileType?"]
                return -code error $msg
            }
            set key "$root\\$extension"
            set fileType [lindex $args 0]
            if {![string length $fileType]} { return [GetKey $key] }
            registry set $key "" $fileType
        }

        default         {
            set msg "bad option \"$option\": must be exists or set"
            return -code error $msg
        }
    }
}

proc installkit::Windows::FileType { option fileType args } {
    set root HKEY_CLASSES_ROOT

    switch -- $option {
        "exists"        {
            set key "$root\\$fileType"
            return [expr [string length [GetKey $key]] > 0]
        }

        "set"           {
            if {[llength $args] > 1} {
                set msg [WrongNumArgs FileExtension "set fileType ?newTitle?"]
                return -code error $msg
            }
            set key "$root\\$fileType"
            set newTitle [lindex $args 0]

            ## If no ?newTitle? was given, return the current title
            ## for this file type.
            if {![string length $newTitle]} { return [GetKey $key] }

            ## Set the new title for this file type.
            registry set $key "" $newTitle

            return $key
        }

        "icon"          {
            if {[llength $args] > 1} {
                set msg [WrongNumArgs FileType "icon fileType ?newIcon?"]
                return -code error $msg
            }
            set key "$root\\$fileType\\DefaultIcon"
            set newIcon [lindex $args 0]

            ## If no ?newIcon? was given, return the current icon
            ## for this file type.
            if {![string length $newIcon]} { return [GetKey $key] }

            ## Set the new icon for this file type.
            registry set $key "" $newIcon

            return $key
        }

        "command"       {
            if {[llength $args] > 2} {
                set str "command fileType ?command? ?newCommand?"
                set msg [WrongNumArgs FileType $str]
                return -code error $msg
            }
            set command [lindex $args 0]

            ## If no ?command? was given, return a list of all the commands
            ## attached to this file type.
            if {![string length $command]} {
                set key "$root\\$fileType\\Shell"
                if {[catch {registry keys $key} result]} { return }
                return $result
            }

            set newCommand [lindex $args 1]
            set key "$root\\$fileType\\Shell\\$command\\command"

            ## If no ?newCommand? was given, return the current command
            ## implementation for this file type and command.
            if {![string length $newCommand]} { return [GetKey $key] }

            ## Set the new command value for this file type.
            registry set $key "" $newCommand

            return $key
        }

        "defaultcommand" {
            if {[llength $args] > 1} {
                set str "defaultcommand fileType ?newDefault?"
                set msg [WrongNumArgs FileType $str]
                return -code error $msg
            }

            set key "$root\\$fileType\\Shell"
            set newDefault [lindex $args 0]
            ## If no ?newDefault? was given, return the default command
            ## attached to this file type.
            if {![string length $newDefault]} { return [GetKey $key] }

            ## Set the new default command value for this file type.
            registry set $key "" $newDefault

            return $key
        }

        "order" {
            if {[llength $args] > 1} {
                set msg [WrongNumArgs FileType "order fileType ?newOrder?"]
                return -code error $msg
            }
            set key "$root\\$fileType\\Shell"
            set newOrder [lindex $args 0]

            ## If no new order was given, return them the current command
            ## order.  If one has not been specified, return them a list
            ## of the commands in order as keys.
            if {![llength $newOrder]} {
                set list [GetKey $key]
                if {[llength $list]} {
                    foreach cmd [split $list ,] {
                        lappend result [string trim $cmd]
                    }
                    return $result
                }
                if {[catch {registry keys $key} result]} { return }
                return $result
            }

            ## Set the new command order for this file type.
            registry set $key "" [join $newOrder ", "]

            return $key
        }

        "menu" {
            if {[llength $args] < 1 || [llength $args] > 2} {
                set str "menu fileType command ?newTitle?"
                set msg [WrongNumArgs FileType $str]
                return -code error $msg
            }
            set command  [lindex $args 0]
            set newTitle [lindex $args 1]
            set key "$root\\$fileType\\Shell\\$command"

            ## If no ?newTitle? was given, return the current menu
            ## title for this file type.
            if {![string length $newTitle]} { return [GetKey $key] }

            ## Set the new menu title for this file type command.
            registry set $key "" $newTitle

            return $key
        }

        "showextension" {
            set str "showextension fileType <always|never> ?0|1?"
            if {[llength $args] < 1 || [llength $args] > 2} {
                set msg [WrongNumArgs FileType $str]
                return -code error $msg
            }
            set type  [lindex $args 0]
            set value [lindex $args 1]
            set key "$root\\$fileType"

            if {($type != "always" && $type != "never")
                || ([string length $value] && $value != 0 && $value != 1) } {
                set msg [WrongNumArgs FileType $str]
                return -code error $msg
            }

            set var [string totitle $type]ShowExt

            ## If no ?value? was given, return the current value.
            if {![string length $value]} {
                if {[catch {registry get $key $var}]} { return 0 }
                return 1
            }

            ## If value is 1, create var as a registry value.
            ## Otherwise, delete it.
            if {$value} {
                registry set $key $var "" sz
            } else {
                catch { registry delete $key $var }
            }
            return $value
        }

        default         {
            set msg "bad option \"$option\": must be "
            append msg "command, exists, icon, menu, order, "
            append msg "set, or showextension"
            return -code error $msg
        }
    }
}

proc installkit::Windows::File { option file args } {
    set root HKEY_CLASSES_ROOT
    set ext  [file extension $file]
    switch -- $option {
        "type" {
            return [FileExtension set $ext]
        }

        "command" {
            if {[llength $args] > 1} {
                set msg [WrongNumArgs File "command commandName"]
                return -code error $msg
            }
            set fileType [FileExtension set $ext]
            set command  [lindex $args 0]
            return [FileType command $fileType $command]
        }

        "commands" {
            set fileType [FileExtension set $ext]
            set key "$root\\$fileType\\Shell"
            if {[catch {registry keys $key} result]} { return }
            return $result
        }

        "defaultcommand" {
            set fileType [FileExtension set $ext]
            return [FileType defaultcommand $fileType]
        }

        "open" {
            set cmd [File command $file open]
            regsub -all {\\} $cmd {\\\\} cmd
            if {![regsub {%1} $cmd $file cmd]} { lappend cmd $file }
            if {[llength $args]} { eval lappend cmd $args }
            eval exec $cmd &
        }

        "print" {
            set cmd [File command $file print]
            regsub -all {\\} $cmd {\\\\} cmd
            if {![regsub {%1} $cmd $file cmd]} { lappend cmd $file }
            if {[llength $args]} { eval lappend cmd $args }
            eval exec $cmd &
        }

        default {
            set msg "bad option \"$option\": must be "
            append msg "command, commands, open, print, or type"
            return -code error $msg
        }
    }
}

package provide installkit::Windows 1.1
