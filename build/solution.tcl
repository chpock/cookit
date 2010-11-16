namespace eval cookit {}

# a<b -> <0
proc cookit::createSolutionsInt {buildmode targets known} {
    variable versions
    variable partParameters
    variable partProviders

    # TODO: use build mode if not empty

    if {[llength $targets] == 0} {
        foreach {pname pversion} $known {
            if {[info exists provided($pname)] && ($provided($pname) != $pversion)} {
		log 2 "Package $pname provided as $provided($pname) != $pversion"
                return [list]
            }
            set provided($pname) $pversion
            foreach {providedname providedversion} [partGetParameter $pname $pversion provides] {
                if {[info exists provided($providedname)] && ($provided($providedname) != $providedversion)} {
		    log 2 "Package $providedname from $pname provided as $provided($providedname) != $providedversion)"
                    return [list]
                }
                set provided($providedname) $providedversion
            }
            foreach {dependname dependversion} [partGetParameter $pname $pversion depends] {
                set depends($dependname) $dependversion
            }
        }

        set reqtargets [list]
        foreach a [lsort [array names depends]] {
            if {(![info exists provided($a)])} {
                lappend reqtargets $a
            }  elseif  {[compareVersions $provided($a) $depends($a)] < 0} {
		log 2 "Package provide-depends fail for $a - $provided($a) < $depends($a)"
                return [list]
            }
        }
        if {[llength $reqtargets] > 0} {
            return [createSolutionsInt $buildmode $reqtargets $known]
        }
    }

    if {[llength $targets] == 0} {
        # TODO: solution
        return [list $known]
    }

    set rc [list]
    set targetidx 0
    set target [lindex $targets 0]

    set currenttargets [lrange $targets 1 end]

    if {![info exists partProviders($target)]} {
	log 2 "Package $target is not provided by anyone"
        return [list]
    }

    array set ka $known
    foreach {tversion pname pversion} $partProviders($target) {
        if {([info exists ka($pname)])} {
            if {$ka($pname) != $pversion} {continue}
            set currentknown $known
        }  else  {
            set currentknown [linsert $known end $pname $pversion]
        }
        set rc [concat $rc [createSolutionsInt $buildmode $currenttargets $currentknown]]
    }

    return $rc
}

proc cookit::compareSolutions {solutionA solutionB} {
    array set sa $solutionA
    array set sb $solutionB

    set rc 0

    foreach a [lsort -unique [concat [array names sa] [array names sb]]] {
        if {[info exists sa($a)] && [info exists sb($a)]} {
            set sc [compareVersions $sa($a) $sb($a)]
            if {$sc > 0} {
                incr rc 1
            }  elseif {$sc < 0} {
                incr rc -1
            }
        }
    }
    
    return $rc
}

proc cookit::prioritizeSolutionDepends {name version dependname dependversion} {
    # TODO: should we check if dependencies fail here?
    set provides($dependname) 1
    foreach {proname proversion} [partGetParameter $name $version provides] {
	set provides($proname) 1
    }
    foreach {depname depversion} [partGetParameter $name $version depends] {
	if {[info exists provides($depname)]} {
            return 1
        }
    }
    return 0
}

proc cookit::prioritizeSolutionComparer {elementA elementB} {
    foreach {nameA versionA} $elementA break
    foreach {nameB versionB} $elementB break
    
    set dAB [prioritizeSolutionDepends $nameA $versionA $nameB $versionB]
    set dBA [prioritizeSolutionDepends $nameB $versionB $nameA $versionA]
    
    if {$dAB && $dBA} {
        error "Package $nameA $versionA and $nameB $versionB have cross-references"
    }  elseif  {$dAB} {
        return 1
    }  elseif  {$dBA} {
        return -1
    }  else  {
        return 0
    }
}

proc cookit::prioritizeSolution {solution} {
    set tsolution [list]
    foreach {name version} $solution {
        lappend tsolution [list $name $version]
    }
    set tsolution [lsort -command cookit::prioritizeSolutionComparer $tsolution]
    set solution [list]
    foreach e $tsolution {
        lappend solution [lindex $e 0] [lindex $e 1]
    }
    return $solution
}

proc cookit::createSolution {buildmode targets} {
    variable opt
    variable versions
    variable partParameters
    
    set known [list]
    foreach name [lsort [array names versions]] {
        if {$opt($name) != ""} {
            lappend known $name $opt($name)
        }
    }

    set list [createSolutionsInt $buildmode $targets $known]
    if {[llength $list] == 0} {
	log 2 "createSolution: known=$known targets=$targets"
	error "Unable to find solution for specified target and criteria"
    }
    set slist [lsort -command cookit::compareSolutions $list]

    set solution [lindex $slist end]
    set solution [prioritizeSolution $solution]

    return $solution
}


