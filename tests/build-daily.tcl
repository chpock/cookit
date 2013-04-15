package require vfs::cookfs

cd [file dirname [info script]]

file delete -force _tmp
file mkdir _tmp

catch {clock format [clock seconds]}
catch {vfs::unmount [info nameofexe]}

foreach platform $argv {
    foreach type {daily daily85} {
        set dest _tmp/$platform-$type
        file mkdir $dest
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
            puts "$platform - $type - $binary"
	    if {![file exists [file join $dir $binary]]} {
		puts stderr "File $dir/$binary does not exist"
		continue
	    }
            file copy -force [file join $dir $binary] $dest/$binary
            set done {}
            vfs::cookfs::Mount $dest/$binary $dest/$binary
            foreach pf [glob -nocomplain $dir/packages/*.cfspkg $dir85/packages/*.cfspkg] {
                set p [lindex [split [file tail $pf] -] 0]
                if {[lsearch -exact $done $p] < 0} {
                    lappend done $p
                    vfs::cookfs::Mount $pf $pf -readonly
                    foreach cf [glob -tails -directory $pf -type f * */* */*/* */*/*/* */*/*/*/* */*/*/*/*/*] {
                        file mkdir $dest/$binary/[file dirname $cf]
                        file copy $pf/$cf $dest/$binary/$cf
                    }
                    vfs::unmount $pf
                }
            }
            vfs::unmount $dest/$binary
        }
    }
}
