namespace eval cookit {}

set cookit::allOptions(ui-delay.arg) {0.0 {Delay for all UI related operations (mainly workaround for NFS/SMB issues)}}

proc cookit::uiImplShowStep {} {
    variable ui
    puts [format "\n\[%2d/%2d\] %s" $ui(partlistcounter) $ui(partlisttotal) $ui(currentstep)]
}

proc cookit::uiImplAddItem {label comments} {
    set label [string replace $label 36 end ...]
    set clk [clock format [clock seconds] -format "\[%H:%M:%S\]"]
    puts "    $clk $label"
    
}

proc cookit::uiImplComplete {} {
    set clk [clock format [clock seconds] -format "\[%H:%M:%S\]"]
    puts "\n\n$clk Command completed successfully\n\n"
}

proc cookit::uiInitializeProgress {partlist} {
    variable ui
    set ui(partlist) $partlist
    set ui(partlistcounter) 0
    set ui(partlisttotal) [llength $partlist]
}

proc cookit::uiSleep {} {
    variable opt
    if {$opt(ui-delay) > 0.0} {
        after [expr {int($opt(ui-delay) * 1000.0)}]
    }
}

proc cookit::uiNextStep {} {
    variable ui

    uiSleep

    set ui(currentstep) [lindex $ui(partlist) 0]
    set ui(partlist) [lrange $ui(partlist) 1 end]
    incr ui(partlistcounter)
    
    uiImplShowStep
}

proc cookit::uiAddItem {label {comments ""}} {
    uiSleep
    uiImplAddItem $label $comments
}

proc cookit::uiComplete {} {
    uiImplComplete
}
