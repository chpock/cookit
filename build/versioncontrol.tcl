namespace eval cookit {}

# retrieving and getting version from various version control systems

proc cookit::cvsCleanupCVSDirectory {dir} {
    log 5 "cvsCleanupCVSDirectory: Cleaning up $dir"
    foreach g [glob -nocomplain -directory $dir *] {
	set gt [file tail $g]
	file stat $g s
	if {($gt == "CVS") && ($s(type) == "directory")} {
	    file delete -force $g
	    continue
	}  elseif {($gt == ".cvsignore") && ($s(type) == "file")} {
	    file delete -force $g
	    continue
	}  elseif  {$s(type) == "directory"} {
	    cvsCleanupCVSDirectory $g
	}
    }
}

proc cookit::cvsExport {address module args} {
    set pwd [pwd]
    cd  [cookit::getTempDirectory]
    set tempdir [pwd]

    set nativeTempdir [file nativename $tempdir]

    log 3 "cvsExport: Checking out $address module $module to $nativeTempdir"

    set cmd [osSpecific cvs]
    lappend cmd "-d${address}"
    lappend cmd "checkout" $module
    set eid [startExtCommand $cmd]
    waitForExtCommand $eid 0

    if {[llength [set g [glob -nocomplain *]]] == 1} {
	log 4 "cvsExport: Only subdirectory is named same as module - assuming repository is in this subdirectory"
	if {[lindex $g 0] == $module} {
	    set tempdir [file join $tempdir $module]
	}
	cvsCleanupCVSDirectory $tempdir
    }

    return $tempdir
}

proc cookit::svnExport {address args} {
    set pwd [pwd]
    cd  [cookit::getTempDirectory]
    set tempdir [pwd]
    cd $pwd

    set nativeTempdir [file nativename $tempdir]

    set cmd [osSpecific svn]
    lappend cmd export --force --non-interactive --quiet
    set cmd [concat $cmd $args]
    lappend cmd $address $tempdir
    set eid [startExtCommand $cmd]
    waitForExtCommand $eid 0
    
    return $tempdir
}

proc cookit::gitExportCleanup {dir} {
    log 5 "cvsCleanupCVSDirectory: Cleaning up $dir"
    foreach g [glob -nocomplain -directory $dir *] {
	set gt [file tail $g]
	file stat $g s
	if {($gt == ".git") && ($s(type) == "directory")} {
	    file delete -force $g
	    continue
	}  elseif  {$s(type) == "directory"} {
	    gitExportCleanup $g
	}
    }
}

proc cookit::gitExport {address} {
    set pwd [pwd]
    cd  [cookit::getTempDirectory]
    set tempdir [pwd]

    set nativeTempdir [file nativename $tempdir]
    set module [lindex [split [string trimright $address /] /] end]

    log 3 "gitExport: Checking out $address module $module to $nativeTempdir"

    set cmd [osSpecific git]
    lappend cmd "clone" $address
    set eid [startExtCommand $cmd]
    waitForExtCommand $eid 0

    if {[llength [set g [glob -nocomplain *]]] == 1} {
	log 4 "gitExport: Only subdirectory is named same as module - assuming repository is in this subdirectory"
	if {[lindex $g 0] == $module} {
	    set tempdir [file join $tempdir $module]
	}
	gitExportCleanup $g
    }

    return $tempdir
}

proc cookit::fossilExport {address {r trunk}} {
    package require http

    set pwd [pwd]
    cd [cookit::getTempDirectory]
    set tempdir [pwd]

    set tempfile [file join $tempdir __temp_[pid]__]

    set url "$address/zip/archive.zip?[http::formatQuery uuid $r]"
    log 3 "fossilExport: Downloading $url"

    downloadURL $url $tempfile

    log 4 "fossilExport: Unzipping $url"
    unzip $tempfile $tempdir

    file delete -force $tempfile

    set g [glob -directory $tempdir -nocomplain -tail *]
 
    if {([llength $g] == 1) && [file isdirectory [lindex $g 0]]} {
        set d [lindex $g 0]
        log 4 "fossilExport: Renaming contents of $d into main directory"
        foreach g [glob -directory $d -tail *] {
            file rename [file join $d $g] $g
        }
        file delete -force $d
    }

    cd $pwd

    return $tempdir
}

proc cookit::getConfigureVersion {path} {
    set filelist [concat \
	[list [file join $path configure.in]] \
	[glob -nocomplain -directory $path */configure.in] \
	]

    foreach file $filelist {
	log 5 "getConfigureVersion: checking $file"
	if {[file exists $file] && ([file type $file] == "file")} {
	    set fh [open $file r]
	    set fc [read $fh]
	    close $fh
	    if {[regexp -line "^\\s*AC_INIT\\(\\\[(.*?)\\\].*?\\\[(.*?)\\\]\\)" $fc - name version]} {
		return [list $name $version]
	    }
	}
    }
    return [list "" ""]
}

proc cookit::copySourceRepository {tempdir package version} {
    variable rootdirectory
    set targetdir [file join $rootdirectory sources $package-$version]

    log 3 "copySourceRepository: Copying $tempdir as $targetdir"

    if {[file exists $targetdir]} {
	log 3 "copySourceRepository: Deleting existing sources in $targetdir"
	foreach g [glob -directory $targetdir -nocomplain *] {
	    log 5 "copySourceRepository: Deleting $g"
	    file delete -force $g
	}
    }

    log 3 "copySourceRepository: Copying sources for $package version $version"
    file mkdir $targetdir
    set g [glob -directory $tempdir *]
    if {([llength $g] == 1) && ([file type [lindex $g 0]] == "directory")} {
	set srcdir [lindex $g 0]
    }  else  {
	set srcdir $tempdir
    }
    log 3 "copySourceRepository: Copying sources from $srcdir"
    foreach g [glob -directory $srcdir *] {
	set dg [file join $targetdir [file tail $g]]
	log 5 "copySourceRepository: Copying $g as $dg"
	file copy -force $g $dg
    }
    log 5 "copySourceRepository: Copying complete"
}
