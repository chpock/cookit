package require vfs::cookfs

fconfigure stdout -translation lf

array set a {
    source ""
    destination ""
    bootstrap 0
    compression zlib
}
array set a $argv

if {$a(source) == ""} {exit 1}
if {$a(destination) == ""} {exit 1}

set a(source) [file normalize [file join [pwd] $a(source)]]
set a(destination) [file normalize [file join [pwd] $a(destination)]]
set a(binary) [file normalize [file join [pwd] [info nameofexecutable]]]

if {![string equal $a(source) $a(binary)]} {
    puts "Mounting $a(source)"
    set fsid [vfs::cookfs::Mount $a(source) $a(source)]
    set pages [set ${fsid}(pages)]
}  else  {
    set pages $cookit::cookitpages
}

set fh [open $a(destination) w]
fconfigure $fh -translation binary
puts -nonewline $fh [$pages gethead]
close $fh

set bootstrap ""
if {$a(bootstrap)} {
    set bootstrap [$pages get 0]
}

if {[catch {
    puts "Mounting $a(destination) - -compression $a(compression) $a(destination) $a(destination)"
    set dfsid [vfs::cookfs::Mount -bootstrap $bootstrap -compression $a(compression) $a(destination) $a(destination)]
    
    foreach g [glob -nocomplain -directory $a(source) *] {
        puts "Copying $g"
        file copy -force $g [file join $a(destination) [file tail $g]]
    }
    
    puts "Unmounting $a(destination)"
    vfs::unmount $a(destination)
}]} {
    puts "Error while repacking binary:\n$::errorInfo"
    exit 1
}

catch {file attributes $a(destination) -permissions 0755}

exit 0
