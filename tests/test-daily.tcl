package require vfs::cookfs

cd [file dirname [info script]]

catch {clock format [clock seconds]}
catch {
    vfs::unmount [info nameofexe]
}

foreach aplatform $argv {
    if {[regexp {^(macosx-universal(?:|64))-(.*?)} $aplatform - platform arch]} {
        set arch [list arch -$arch]
    }  else  {
        set arch {}
	set platform $aplatform
    }
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
            puts -nonewline [format %-50s "Test $aplatform $binary $ver:"]
            flush stdout
            set ok 0
            set outfh [open ../_log/test-$aplatform-$type-$ver.txt w]
            puts $outfh "binary=$binary arch=$arch"
            if {[catch {
                exec {*}$arch [file join $dest $binary] all.tcl 2>@1
            } res]} {
                set res "Error:\n$res"
            }  elseif  {[regexp -line {Failed\s+0\s*$} $res]} {
                puts "OK"
                set ok 1
            }
            puts $outfh $res
            close $outfh
            if {!$ok} {
                puts "FAILED"
            }
        }
    }
}
