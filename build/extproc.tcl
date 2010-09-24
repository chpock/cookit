namespace eval cookit {}

set ::cookit::extId 0

proc cookit::extCommandReadable {id} {
    upvar #0 $id v
    variable platform
    variable stderrfilename

    if {[catch {gets $v(fh) line} rc]} {
        set eof 1
    }  else  {
        if {$rc >= 0} {
            log 2 " >> $line"
        }
        set eof 1; catch {set eof [eof $v(fh)]}
    }
    if {$eof} {
        log 5 "extCommandReadable: Binary execution completed"
        if {($platform == "win32-x86") && [file exists $stderrfilename]} {
            if {[catch {
                log 5 "extCommandReadable: Reading stderr messages"
                set fh [open $stderrfilename r]
                fconfigure $fh -buffering line -translation binary -blocking 0
                while {![eof $fh]} {
                    if {[gets $fh line] >= 0} {
                        log 2 " >> $line"
                    }  else  {
                        break
                    }
                }
                close $fh
            } error]} {
                log 1 "Error reading stderr: $error"
            }
            catch {file delete -force $stderrfilename}
        }
        set v(exitcode) -1
        if {[catch {
            fconfigure $v(fh) -blocking 1 -buffering none
            if {[catch {close $v(fh)} error]} {
                set ecode $::errorCode
                log 5 "extCommandReadable: Child exit code information: $ecode"
                if {[string equal [lindex $ecode 0] "NONE"]} {
                    set v(exitcode) 0
                }  elseif {[string equal [lindex $ecode 0] "CHILDSTATUS"]} {
                    set v(exitcode) [lindex $ecode 2]
                }  elseif {[string equal [lindex $ecode 0] "CHILDKILLED"]} {
                    set v(exitcode) -2
                }
            }  else  {
                set v(exitcode) 0
            }
        }]} {
            log 5 "extCommandReadable: Error while retrieving exit code: $::errorInfo"
        }
        log 4 "extCommandReadable: Binary execution completed with exit code $v(exitcode)"
        set v(done) 1
    }
}

proc cookit::startExtCommand {command} {
    variable platform
    variable stderrfilename
    
    log 5 "startExtCommand: Environment variables:"
    foreach n [lsort [array names ::env]] {
        log 5 "  env $n = '$::env($n)'"
    }
    log 2 "startExtCommand: Running $command"
    
    if {$platform == "win32-x86"} {
        #lappend command "2>@" "stdout"
        lappend command "2>" $stderrfilename
    }  else  {
        lappend command "2>@" "stdout"
    }

    set fh [open "|$command" r+]

    set id ::cookit::ext[incr ::cookit::extId]
    upvar #0 $id v
    set v(fh) $fh
    set v(done) 0
    fconfigure $fh -buffering line -translation binary -blocking 0
    fileevent $fh readable [list cookit::extCommandReadable $id]
    return $id
}

proc cookit::waitForExtCommand {id args} {
    upvar #0 $id v
    vwait ${id}(done)
    set rc $v(exitcode)
    
    if {[llength $args] > 0} {
        cleanupExtCommand $id
        if {[lsearch -exact $args $rc] < 0} {
            set e "Error: Command exited with code $rc"
            log 2 "waitForExtCommand: $e"
            return -code error -errorinfo $e $e
        }
    }

    return $rc
}

proc cookit::cleanupExtCommand {id} {
    unset $id
}
