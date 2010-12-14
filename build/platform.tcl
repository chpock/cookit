namespace eval cookit {}

proc cookit::setPlatform {} {
    variable platform

    set p "$::tcl_platform(machine)-$::tcl_platform(os)-$::tcl_platform(osVersion)"
    if {[string match "i*86*-Linux-*" $p] || [string match "intel-Linux-*" $p]} {
	set platform linux-x86
    }  elseif {[string match "ppc*-Linux-*" $p]} {
	set platform linux-ppc
    }  elseif {[string match "s390*-Linux-*" $p]} {
	set platform linux-s390
    }  elseif {[string match "alpha*-Linux-*" $p]} {
	set platform linux-alpha
    }  elseif {[string match "arm*-Linux-*" $p]} {
	set platform linux-arm
    }  elseif {[string match "i*86*-Windows*-*" $p] || [string match "intel-Windows*-*" $p]} {
	set platform win32-x86
    }  elseif {[string match "i*86*-Darwin-*" $p] || [string match "intel-Darwin-*" $p]} {
	set platform macosx-x86
    }  elseif {[string match "Power*-Darwin-*" $p]} {
	set platform macosx-ppc
    }  elseif {[string match "i*86*-SunOS-*" $p] || [string match "intel-SunOS-*" $p]} {
	set platform solaris-x86
    }  elseif {[string match "sun*-SunOS-*" $p]} {
	set platform solaris-sparc
    }  elseif {[string match "*-AIX-*" $p]} {
	set platform aix-ppc
    }  elseif {[string match "ia64*-HP-UX-*" $p]} {
	set platform hpux-ia64
    }  elseif {[string match "*-HP-UX-*" $p]} {
	set platform hpux-parisc
    }  elseif {[string match "i*86*-FreeBSD*-*" $p] || [string match "intel-FreeBSD*-*" $p]} {
	set platform freebsd-x86
    }  elseif {[string match "i*86*-NetBSD*-*" $p] || [string match "intel-NetBSD*-*" $p]} {
	set platform netbsd-x86
    }  elseif {[string match "i*86*-OpenBSD*-*" $p] || [string match "intel-OpenBSD*-*" $p]} {
	set platform openbsd-x86
    }  else  {
	error "Unknown platform $p"
    }
}

