namespace eval cookit {}

proc cookit::setPlatform {} {
    variable platform

    set p "$::tcl_platform(machine)-$::tcl_platform(os)-$::tcl_platform(osVersion)"
    if {[string match "i*86*-Linux-*" $p] || [string match "intel-Linux-*" $p]} {
	set platform linux-x86
    }  elseif {[string match "i*86*-Windows*-*" $p] || [string match "intel-Windows*-*" $p]} {
	set platform win32-x86
    }  elseif {[string match "i*86*-SunOS-*" $p] || [string match "intel-SunOS-*" $p]} {
	set platform solaris-x86
    }  elseif {[string match "i*86*-Darwin-*" $p] || [string match "intel-Darwin-*" $p]} {
	set platform macosx-x86
    }  else  {
	error "Unknown platform $p"
    }
}

