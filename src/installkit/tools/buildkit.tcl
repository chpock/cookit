## buildkit.tcl --
##
## Script to build the initial installkit from all the separate components.
##

proc recursive_glob { dir pattern } {
    set files [glob -nocomplain -dir $dir -type f $pattern]
    foreach dir [glob -nocomplain -dir $dir -type d *] {
        eval lappend files [recursive_glob $dir $pattern]
    }
    return $files
}

proc build { installkitBin method files names } {
    set fp [miniarc::open crap $installkitBin a -method $method]
    if {[catch { miniarc::addfiles $fp $files $names } error]} {
        return -code error $error
    }

    miniarc::close $fp
}

proc main { installkitBin vfsDir } {
    package require miniarc

    set manifest [file join $vfsDir lib installkit manifest.txt]
    set fp [open $manifest]
    set x  [read $fp]
    close $fp

    foreach file [lsort -decreasing -dictionary $x] {
        lappend files [file join $vfsDir $file]
        lappend names $file
    }

    if {[catch { build $installkitBin lzma $files $names }]} {
        build $installkitBin zlib $files $names
    }
}

eval main $argv
