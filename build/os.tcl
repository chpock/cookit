namespace eval cookit {}

# OS specific workarounds and resolutions

proc cookit::shellScriptCommand {cmd} {
    variable platform
    
    if {$platform == "win32-x86"} {
        set command [list sh -c [linsert $cmd 0 sh]]
    }  else  {
        set command [linsert $cmd 0 sh]
    }

    log 5 "shellScriptCommand: Converted \"$cmd\" to \"$command\""

    return $command
}

proc cookit::osSpecific {value} {
    variable platform
    variable msysdirectory

    set v $value

    switch -glob -- $value,$platform {
        .,win32-x86 - .,solaris-* {
            set value ""
        }
        
    }

    return $value
}

proc cookit::initWin32Msys {directory} {
    variable rootdirectory
    variable msysdirectory
    set dir [mkrelative $rootdirectory $directory]

    if {![file exists $dir]} {
	set zip [mkrelative $rootdirectory $directory.zip]
	file mkdir [file dirname $zip]
	# try to unzip msys.zip
	if {[catch {
	    package require vfs::zip

	    if {![file exists $zip]} {
		set url "http://sourceforge.net/projects/cookit/files/win32-tools-msys.zip/download"
		log 5 "msys.zip not found - downloading from '$url'"
		if {![downloadURL $url $zip]} {
		    error "download of $url failed"
		}
	    }

	    vfs::zip::Mount $zip $zip
	    catch {file delete -force $dir}
	    catch {file delete -force $dir.tmp}
	    file mkdir $dir.tmp
	    foreach g [glob -directory $zip *] {
		set d [file join $dir.tmp [file tail $g]]
		log 5 "Copying '$g' as '$d'"
		file copy -force $g $d
	    }
	    vfs::unmount $zip
	    file rename $dir.tmp $dir
	    exit 12
	} error]} {
	    # TODO: better description
	    puts stderr "Unable to download and unpack msys.zip file: $error"
	    puts stderr ""
	    puts stderr "This file is needed on win32 in order to properly build cookit"
	    exit 1
	}
    }

    set msysdirectory $directory

    set paths [list]
    lappend paths [file nativename [file join $directory bin]]
    lappend paths [file nativename [file join $directory mingw bin]]
    
    foreach path [split $::env(PATH) ";"] {
        if {[file exists [file join $path cygpath.exe]]
            || [file exists [file join $path cygwin1.dll]]
            || [file exists [file join $path cygXft-1.lnk]]
        } {
            continue
        }
        lappend paths $path
    }
    set ::env(PATH) [join $paths ";"]
    
    set ::env(MSYSTEM) "MINGW32"
}

proc cookit::initOS {} {
    variable rootdirectory
    variable hostname
    variable hostbuilddirectory
    variable platform
    variable unixwinplatform
    variable unixwinmacosxplatform
    variable opt

    if {(![info exists hostname]) || ($hostname == "")} {
        set hostname [lindex [split [info hostname] .:] 0]
    }

    if {![info exists platform]} {
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

    if {[string match macosx-* $platform]} {
        set unixwinplatform "unix"
        set unixwinmacosxplatform "macosx"
    }  elseif {$platform == "win32-x86"} {
        set unixwinplatform "win"
        set unixwinmacosxplatform "win"
    }  else  {
        set unixwinplatform "unix"
        set unixwinmacosxplatform "unix"
    }

    if {$platform == "win32-x86"} {
        initWin32Msys [file join $rootdirectory win32 msys]
    }
}
