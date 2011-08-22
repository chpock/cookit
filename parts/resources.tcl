namespace eval cookit::resources {}

set cookit::allOptions(resource-name.arg) {{cookit} {Resource file suffix to use}}
cookit::partRegister resources "Win32 resources"

proc cookit::resources::parameters {version} {
    return [list \
        provides {} depends {tcl ""} \
        buildmodes {static} \
        ]
}

proc cookit::resources::initialize-static {version} {
}

proc cookit::resources::configure-static {} {
}

proc cookit::resources::build-static {} {
    if {$::cookit::platform == "win32-x86"} {
        if {[cookit::isPartIncluded tk]} {
	    set suffix "-tk"
	    set tkdefine "COOKIT_USE_TK"
	} else {
	    set suffix "-notk"
	    set tkdefine "COOKIT_DONT_USE_TK"
	}

        set tcldir [cookit::getSourceDirectory tcl]
	# same Tk version has to be used anyway, so we simply map tcl- to tk-
	set tkdir [file join [file dirname $tcldir] [string map {tcl- tk-} [file tail $tcldir]]]
        set sdir [cookit::getSourceDirectory resources]

        set cmd  [list windres -o resource$suffix.o \
            --define STATIC_BUILD \
            --define $tkdefine \
            --include $sdir \
            --include [file join $tcldir generic] \
            --include [file join $tcldir win] \
            --include [file join $tkdir generic] \
            --include [file join $tkdir win] \
            --include [file join $tkdir win rc] \
            [file join $sdir resource-$::cookit::opt(resource-name)$suffix.rc] \
            ]
        set eid [cookit::startExtCommand $cmd]
        cookit::waitForExtCommand $eid 0
    }
}

proc cookit::resources::install-static {} {
    if {$::cookit::platform == "win32-x86"} {
    }
}

proc cookit::resources::vfsfilelist-static {} {
    set filelist [list]
    if {$::cookit::platform == "win32-x86"} {
        set dir [cookit::getSourceDirectory resources]
        lappend filelist cookit.ico file [file join $dir cookit.ico] ""
    }
    return $filelist
}

proc cookit::resources::linkerflags-static {} {
    set rc [list]

    if {$::cookit::platform == "win32-x86"} {
    }
    return $rc
}

proc cookit::resources::linkerfiles-static {} {
    set rc [list]
    
    if {$::cookit::platform == "win32-x86"} {
        if {[cookit::isPartIncluded tk]} {set suffix "-tk"} else {set suffix "-notk"}
        lappend rc [file join [cookit::getBuildStaticDirectory resources] resource$suffix.o]
    }
    return $rc
}
