namespace eval cookit {}

set cookit::allOptions(loglevel.arg) {3 {Level of logging to output; 1-5}}
set cookit::allOptions(logdebug) {{Print all logs to stdout}}

proc cookit::logInit {step} {
    variable rootdirectory 
    variable hostbuilddirectory

    variable log
    set log(step) $step
    close [open [file join $rootdirectory _log $hostbuilddirectory $log(step).txt] w]
}

proc cookit::log {level message {data ""}} {
    variable log
    variable opt
    variable rootdirectory 
    variable hostbuilddirectory

    if {[info exists ::env(COOKITBUILDDEBUG)] || $opt(logdebug)} {
        puts "\[[clock format [clock seconds] -format %H:%M:%S]\] $level $message"
    }

    if {[info exists log(step)]} {
        if {$opt(loglevel) >= $level} {
            set fh [open [file join $rootdirectory _log $hostbuilddirectory $log(step).txt] a]
            puts $fh "\[[clock format [clock seconds] -format %H:%M:%S]\] $level $message"
            close $fh
        }
    }
}
