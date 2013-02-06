namespace eval cookit {}

# OS specific workarounds and resolutions

set cookit::allOptions(usemingw) {Use MinGW and SVN provided on SourceForge.net/projects/cookit}
set cookit::allOptions(mingw.arg) {{} Use specific version of mingw}
set cookit::allOptions(hostname.arg) [list [lindex [split [info hostname] .:] 0] [list Force specific hostname]]

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
    variable rootdirectory

    set v $value

    if {![info exists ::env(CRITCL)]} {
        set ::env(CRITCL) [file normalize [file join $rootdirectory tools critcl]]
    }

    if {($value == "gcc") || ($value == "cc")} {
        if {[info exists ::env(CC)]} {
            return $::env(CC)
        }
    }

    switch -glob -- $value,$platform {
        .,win32-x86 - .,solaris-* {
            set value ""
        }
	make,solaris-* {
	    set value "gmake"
	}
        
    }

    return $value
}

proc cookit::initWin32Svn {directory} {
    variable rootdirectory

    set dir [mkrelative $rootdirectory $directory]
    if {![file exists $dir]} {
	puts "Downloading win32 subversion client; this will take a few minutes"
	set zip [mkrelative $rootdirectory $directory.zip]
	file mkdir [file dirname $zip]

	if {[catch {
	    package require vfs::zip

	    if {![file exists $zip]} {
		set url "http://subversion.tigris.org/files/documents/15/47914/svn-win32-1.6.6.zip"
		log 5 "[file tail $zip] not found - downloading from '$url'"
		if {![downloadURL $url $zip]} {
		    error "download of $url failed"
		}
	    }

	    vfs::zip::Mount $zip $zip
	    catch {file delete -force $dir}
	    catch {file delete -force $dir.tmp}
	    file mkdir $dir.tmp
	    foreach g [glob -directory $zip svn-*/bin svn-*/iconv] {
		set d [file join $dir.tmp [file tail $g]]
		log 5 "Copying '$g' as '$d'"
		file copy -force $g $d
	    }
	    vfs::unmount $zip
	    file rename $dir.tmp $dir
	} error]} {
	    # TODO: better description
	    puts stderr "Unable to download and unpack subversion client: $error"
	    puts stderr ""
	    puts stderr "This file is needed on win32 in order to properly build cookit"
	    exit 1
	}
    }

    set ::env(PATH) "[file nativename [file join $directory bin]];$::env(PATH)"
}

proc cookit::initWin32Msys {directory mingwsuffix} {
    variable rootdirectory
    variable msysdirectory

    set dir [mkrelative $rootdirectory $directory]
    if {![file exists $dir]} {
	puts "Downloading win32 build tools; this will take a few minutes"
	set zip [mkrelative $rootdirectory $directory.zip]
	file mkdir [file dirname $zip]
	# try to unzip msys.zip
	if {[catch {
	    package require vfs::zip

	    if {![file exists $zip]} {
		set url "http://sourceforge.net/projects/cookit/files/win32-tools-msys$mingwsuffix.zip/download"
		log 5 "msys.zip not found - downloading from '$url'"
		if {![downloadURL $url $zip]} {
		    error "download of $url failed"
		}
	    }

	    vfs::zip::Mount $zip $zip
	    catch {file delete -force $dir}
	    catch {file delete -force $dir.tmp}
	    file mkdir $dir.tmp
	    foreach g [glob -directory $zip msys/*] {
		set d [file join $dir.tmp [file tail $g]]
		log 5 "Copying '$g' as '$d'"
		file copy -force $g $d
	    }
	    vfs::unmount $zip
	    file rename $dir.tmp $dir
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
    lappend paths [file nativename [file join $directory mingw bin]]
    lappend paths [file nativename [file join $directory bin]]
    
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

proc cookit::preInitOS {} {
    variable platform
    variable unixwinplatform
    variable unixwinmacosxplatform
    variable opt

    if {![info exists platform]} {
	setPlatform
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
}

proc cookit::initOS {} {
    variable hostname
    variable platform
    variable rootdirectory
    variable opt

    if {(![info exists hostname]) || ($hostname == "")} {
        set hostname $opt(hostname)
    }

    if {$platform == "win32-x86"} {
	if {$opt(usemingw)} {
	    if {$opt(mingw) != ""} {
		set mingwsuffix "-$opt(mingw)"
	    }  else  {
		set mingwsuffix ""
	    }

	    set msysdir "msys$mingwsuffix"
	
	    initWin32Msys [file join $rootdirectory win32 $msysdir] $mingwsuffix
	    initWin32Svn [file join $rootdirectory win32 svn]
	}
    }
}

