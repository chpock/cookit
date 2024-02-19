# Various utility routines used in the TWAPI tests

global psinfo;                    # Array storing process information

global thrdinfo;                  # Array storing thread informations

proc load_twapi {} {
    if {[catch {package require twapi}]} {
        set dir [file dirname [info script]]
        if {[file exists [file join $dir twapi.kit]]} {
            set kitfile [file join $dir twapi.kit]
        } else {
            if {[file exists [file join $dir .. twapi.kit]]} {
                set kitfile [file join $dir .. twapi.kit]
            }        
        }
        if {[info exists kitfile]} {
            source $kitfile
            package require twapi
        } else {
            # Last resort
            source [file join [file dirname [info script]] "../tcl/twapi.tcl"]
        }
    }
}

# From http://mini.net/tcl/460
#
# If you need to split string into list using some more complicated rule
# than builtin split command allows, use following function
# It mimics Perl split operator which allows regexp as element
# separator, but, like builtin split, it expects string to split as
# first arg and regexp as second (optional) By default, it splits by any
# amount of whitespace.

# Note that if you add parenthesis into regexp, parenthesed part of
# separator would be added into list as additional element. Just like in
# Perl. -- cary
proc xsplit [list str [list regexp "\[\t \r\n\]+"]] {
    set list  {}
    while {[regexp -indices -- $regexp $str match submatch]} {
        lappend list [string range $str 0 [expr [lindex $match 0] -1]]
        if {[lindex $submatch 0]>=0} {
            lappend list [string range $str [lindex $submatch 0]\
                              [lindex $submatch 1]]
        }
        set str [string range $str [expr [lindex $match 1]+1] end]
    }
    lappend list $str
    return $list
}

# Validate IP address
proc valid_ip_address {ipaddr} {
    # (Copied from Mastering Regular Expression)
    # Expression to match 0-255
    set sub {([01]?\d\d?|2[0-4]\d|25[0-5])}

    return [regexp "^$sub\.$sub\.$sub\.$sub\$" $ipaddr]
}

# Validate list of ip addresses
proc validate_ip_addresses {addrlist} {
    foreach addr $addrlist {
        if {![valid_ip_address $addr]} {return 0}
    }
    return 1
}


# Start notepad and wait till it's up and running.
proc notepad_exec {args} {
    set pid [eval [list exec notepad.exe] $args &]
    if {![twapi::process_waiting_for_input $pid -wait 5000]} {
        error "Timeout waiting for notepad to be ready for input"
    }
    return $pid
}

# Start notepad, make it store something in the clipboard and exit
proc notepad_copy {text} {
    set pid [notepad_exec]
    set hwin [twapi::find_windows -pids [list $pid] -class Notepad]
    twapi::set_foreground_window $hwin
    after 100;                          # Wait for it to become foreground
    twapi::send_keys $text
    twapi::send_keys ^a^c;                 # Select all and copy
    after 100
    twapi::end_process $pid -force
}


proc get_processes {{refresh 0}} {
    global psinfo
    
    if {[info exists psinfo(0)] && ! $refresh} return
    
    catch {unset psinfo}
    array set psinfo {} 
    set fd [open "| cscript.exe /nologo process.vbs"]
    while {[gets $fd line] >= 0} {
        if {[string length $line] == 0} continue
        array set processinfo [split $line "*"]
        set pid $processinfo(ProcessId)
        set psinfo($pid) \
            [list \
                 -basepriority $processinfo(Priority) \
                 -handlecount  $processinfo(HandleCount) \
                 -name         $processinfo(Name) \
                 -pagefilebytes $processinfo(PageFileUsage) \
                 -pagefilebytespeak $processinfo(PeakPageFileUsage) \
                 -parent       $processinfo(ParentProcessId) \
                 -path         $processinfo(ExecutablePath) \
                 -pid          $pid \
                 -poolnonpagedbytes $processinfo(QuotaNonPagedPoolUsage) \
                 -poolpagedbytes $processinfo(QuotaPagedPoolUsage) \
                 -privatebytes $processinfo(PrivatePageCount) \
                 -threadcount  $processinfo(ThreadCount) \
                 -virtualbytes     $processinfo(VirtualSize) \
                 -virtualbytespeak $processinfo(PeakVirtualSize) \
                 -workingset       $processinfo(WorkingSetSize) \
                 -workingsetpeak   $processinfo(PeakWorkingSetSize) \
                 -user             $processinfo(User) \
                ]
    }
    close $fd
}

# Get given field for the given pid. Error if pid does not exist
proc get_process_field {pid field {refresh 0}} {
    global psinfo
    get_processes $refresh
    if {![info exists psinfo($pid)]} {
        error "Pid $pid does not exist"
    }
    return [get_kl_field $psinfo($pid) $field]
}

# Get the first pid with the given value (case insensitive)
# in the given field
proc get_process_with_field_value {field value {refresh 0}} {
    global psinfo
    get_processes $refresh
    foreach pid [array names psinfo] {
        if {[string equal -nocase [get_process_field $pid $field] $value]} {
            return $pid
        }
    }
    error "No process with $field=$value"
}

proc get_winlogon_path {} {
    set winlogon_path [file join $::env(WINDIR) "system32" "winlogon.exe"]
    return [string tolower [file nativename $winlogon_path]]
}

proc get_winlogon_pid {} {
    global winlogon_pid
    if {! [info exists winlogon_pid]} {
        set winlogon_pid [get_process_with_field_value -name "winlogon.exe"]
    }
    return $winlogon_pid
}

proc get_explorer_path {} {
    set explorer_path [file join $::env(WINDIR) "explorer.exe"]
    return [string tolower [file nativename $explorer_path]]
}

proc get_explorer_pid {} {
    global explorer_pid
    if {! [info exists explorer_pid]} {
        set explorer_pid [get_process_with_field_value -name "explorer.exe"]
    }
    return $explorer_pid
}

proc get_explorer_tid {} {
    return [get_thread_with_field_value -pid [get_explorer_pid]]
}

proc get_notepad_path {} {
    set path [file join $::env(WINDIR) "notepad.exe"]
    return [string tolower [file nativename $path]]
}

proc get_cmd_path {} {
    set path [file join $::env(WINDIR) system32 "cmd.exe"]
    return [string tolower [file nativename $path]]
}

proc get_system_pid {} {
    global system_pid
    global psinfo
    if {! [info exists system_pid]} {
        set system_pid [get_process_with_field_value -name "System"]
    }
    return $system_pid
}

proc get_idle_pid {} {
    global idle_pid
    global psinfo
    if {! [info exists idle_pid]} {
        set idle_pid [get_process_with_field_value -name "System Idle Process"]
    }
    return $idle_pid
}

proc get_threads {{refresh 0}} {
    global thrdinfo
    
    if {[info exists thrdinfo] && ! $refresh} return
    catch {unset thrdinfo}
    array set thrdinfo {} 
    set fd [open "| cscript.exe /nologo thread.vbs"]
    while {[gets $fd line] >= 0} {
        if {[string length $line] == 0} continue
        array set threadrec [split $line "*"]
        set tid $threadrec(Handle)
        set thrdinfo($tid) \
            [list \
                 -tid $tid \
                 -basepriority $threadrec(PriorityBase) \
                 -pid          $threadrec(ProcessHandle) \
                 -priority     $threadrec(Priority) \
                 -startaddress $threadrec(StartAddress) \
                 -state        $threadrec(ThreadState) \
                 -waitreason   $threadrec(ThreadWaitReason) \
                ]
    }
    close $fd
}

# Get given field for the given tid. Error if tid does not exist
proc get_thread_field {tid field {refresh 0}} {
    global thrdinfo
    get_threads $refresh
    if {![info exists thrdinfo($tid)]} {
        error "Thread $tid does not exist"
    }
    return [get_kl_field $thrdinfo($tid) $field]
}

# Get the first tid with the given value (case insensitive)
# in the given field
proc get_thread_with_field_value {field value {refresh 0}} {
    global thrdinfo
    get_threads $refresh
    foreach tid [array names thrdinfo] {
        if {[string equal -nocase [get_thread_field $tid $field] $value]} {
            return $tid
        }
    }
    error "No thread with $field=$value"
}

# Get list of threads for the given process
proc get_process_tids {pid {refresh 0}} {
    global thrdinfo
    
    get_threads $refresh
    set tids [list ]
    foreach {tid rec} [array get thrdinfo] {
        array set thrd $rec
        if {$thrd(-pid) == $pid} {
            lappend tids $tid
        }
    }
    return $tids
}


# Start the specified program and return its pid
proc start_program {exepath args} {
    set pid [eval exec [list $exepath] $args &]
    # Wait to ensure it has started up
    if {![twapi::wait {twapi::process_exists $pid} 1 1000]} {
        error "Could not start $exepath"
    }
    # delay to let it get fully initialized.
    after 100
    return $pid
}

# Compare two strings as paths
proc equal_paths {p1 p2} {
    # Use file join to convert \ to /
    return [string equal -nocase [file join $p1] [file join $p2]]
}


# Return a field is keyed list
proc get_kl_field {kl field} {
    foreach {fld val} $kl {
        if {$fld == $field} {
            return $val
        }
    }
    error "No field $field found in keyed list"
}

#
# Verify that a keyed list has the specified fields and no others
# Raises an error otherwise
proc verify_kl_fields {kl fields} {
    array set data $kl
    foreach field $fields {
        if {![info exists data($field)]} {
            puts stderr "Field $field not found keyed list"
            error "Field $field not found keyed list"
        }
        unset data($field)
    }
    set extra [array names data]
    if {[llength $extra]} {
        puts stderr "Extra fields ([join $extra ,]) found in keyed list"
        error "Extra fields ([join $extra ,]) found in keyed list"
    }
    return
}

#
# Verify that all elements in a list of keyed lists have
# the specified fields and no others
# Raises an error otherwise
proc verify_list_kl_fields {l fields} {
    foreach kl $l {
        verify_kl_fields $kl $fields
    }
}

#
# Verify is an integer pair
proc verify_integer_pair {pair} {
    if {([llength $pair] != 2) || 
        (![string is integer [lindex $pair 0]]) ||
        (![string is integer [lindex $pair 1]]) } {
        error "'$pair' is not a pair of integers"
    }
    return
}

# Return true if all items in a list look like privileges
proc verify_priv_list {privs} {
    set match 1
    foreach priv $privs {
        set match [expr {$match && [string match Se* $priv]}]
    }
    return $match
}


#####
#
# "SetOps, Code, 8.x v2"
# http://wiki.tcl.tk/1763
#
#
#####


# ---------------------------------------------
# SetOps -- Set operations for Tcl
#
# (C) c.l.t. community, 1999
# (C) TclWiki community, 2001
#
# ---------------------------------------------
# Implementation variant for tcl 8.x and beyond.
# Uses namespaces and 'unset -nocomplain'
# ---------------------------------------------
# NOTE: [set][array names] in the {} array is faster than
#   [set][info locals] for local vars; it is however slower
#   for [info exists] or [unset] ...

namespace eval ::setops {
    namespace export {[a-z]*}
}

proc ::setops::create {args} {
    cleanup $args
}

proc ::setops::cleanup {A} {
    # unset A to avoid collisions
    foreach [lindex [list $A [unset A]] 0] {.} {break}
    info locals
}

proc ::setops::union {args} {
    switch [llength $args] {
	 0 {return {}}
	 1 {return [lindex $args 0]}
    }

   foreach setX $args {
	foreach x $setX {set ($x) {}}
   }
   array names {}
}

proc ::setops::diff {A B} {
    if {[llength $A] == 0} {
	 return {}
    }
    if {[llength $B] == 0} {
	 return $A
    }

    # get the variable B out of the way, avoid collisions
    # prepare for "pure list optimisation"
    set ::setops::tmp [lreplace $B -1 -1 unset -nocomplain]
    unset B

    # unset A early: no local variables left
    foreach [lindex [list $A [unset A]] 0] {.} {break}

    eval $::setops::tmp

    info locals
}

proc ::setops::contains {set element} {
   expr {[lsearch -exact $set $element] < 0 ? 0 : 1}
}

proc ::setops::symdiff {A B} {
    union [diff $A $B] [diff $B $A]
}

proc ::setops::empty {set} {
   expr {[llength $set] == 0}
}

proc ::setops::intersect {args} {
   set res  [lindex $args 0]
   foreach set [lrange $args 1 end] {
	if {[llength $res] && [llength $set]} {
	    set res [Intersect $res $set]
	} else {
	    break
	}
   }
   set res
}

proc ::setops::Intersect {A B} {
# This is slower than local vars, but more robust
    if {[llength $B] > [llength $A]} {
	 set res $A
	 set A $B
	 set B $res
    }
    set res {}
    foreach x $A {set ($x) {}}
    foreach x $B {
	 if {[info exists ($x)]} {
	     lappend res $x
	 }
    }
    set res
}


# EOF


