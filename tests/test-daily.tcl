package require vfs::cookfs

cd [file dirname [info script]]

catch {clock format [clock seconds]}
catch {
    vfs::unmount [info nameofexe]
}

foreach platform $argv {
    foreach {type ver} {daily 8.6 daily85 8.5} {
        set dest _tmp/$platform-$type
        set dir [file normalize [file join [pwd] ../_output $platform-$type]]
        set dir85 [file normalize [file join [pwd] ../_output $platform-daily85]]
        if {$platform == "win32-x86"} {
            set binaries {cookit.exe cookit-ui.exe}
        }  elseif {$platform == "solaris-x86" || $platform == "solaris-sparc"} {
            set binaries {cookit cookit-tls}
        }  else  {
            set binaries {cookit}
        }
        foreach binary $binaries {
            puts -nonewline [format %-50s "Test $platform $binary $ver:"]
            flush stdout
            set ok 0
            if {[catch {
                exec [file join $dest $binary] all.tcl 2>@1
            } res]} {
            }  elseif  {[regexp -line {Failed\s+0\s*$} $res]} {
                puts "OK"
                set ok 1
            }
            if {!$ok} {
                puts "FAILED"
            }
        }
    }
}
