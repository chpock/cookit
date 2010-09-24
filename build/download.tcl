namespace eval cookit {}

proc cookit::downloadURLProgress {token total current} {
    log 5 "Download progress: $current / $total"
}
proc cookit::downloadURL {url {destinationfile ""}} {
    package require http

    set startURL $url
    set retries 10
    if {$destinationfile != ""} {
        set result 0
    }

    while {[incr retries -1] >= 0} {
        log 3 "Downloading $url (retries=$retries)"
        if {[catch {
            set h [http::geturl $url -binary 1 -progress cookit::downloadURLProgress]
        }]} {
            set h [http::geturl $url -progress cookit::downloadURLProgress]
        }

        upvar #0 $h t

        if {$t(status) != "ok"} {
            log 2 "Status is $t(status); starting from beginning"
            http::cleanup $h
            set url $startURL
            continue
        }
        if {[info exists t(meta)]} {
            catch {unset meta}
            array set meta $t(meta)
        }
        set code [lindex [split $t(http)] 1]

        if {($code == 302) || ($code == 303)} {
            if {[info exists meta(Location)]} {
                set url $meta(Location)
                http::cleanup $h
                continue
            }  elseif {[info exists meta(location)]} {
                set url $meta(location)
                http::cleanup $h
                continue
            }  else  {
                log 2 "Redirect requested, but no location specified; starting from beginning"
                set url $startURL
                http::cleanup $h
                continue
            }
        }  else  {
            if {$destinationfile != ""} {
                set fh [open $destinationfile w]
                fconfigure $fh -translation binary
                puts -nonewline $fh $t(body)
                close $fh
                set result 1
            }  else  {
                set result $t(body)
            }
            break
        }
    }
    return $result
}

