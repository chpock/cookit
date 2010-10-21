namespace eval cookit {}

package require Tcl 8.3

cd [file dirname [info script]]
cd ..

set cookit::rootdirectory [pwd]
set ::env(LANG) C

source [file join $cookit::rootdirectory build libs cmdline.tcl]
source [file join $cookit::rootdirectory build libs md5.tcl]

source [file join $cookit::rootdirectory build init.tcl]
source [file join $cookit::rootdirectory build os.tcl]
source [file join $cookit::rootdirectory build platform.tcl]
source [file join $cookit::rootdirectory build log.tcl]
source [file join $cookit::rootdirectory build versioncontrol.tcl]
source [file join $cookit::rootdirectory build download.tcl]
source [file join $cookit::rootdirectory build parts.tcl]
source [file join $cookit::rootdirectory build common.tcl]
source [file join $cookit::rootdirectory build ui.tcl]
source [file join $cookit::rootdirectory build extproc.tcl]
source [file join $cookit::rootdirectory build commands.tcl]
source [file join $cookit::rootdirectory build solution.tcl]
source [file join $cookit::rootdirectory build operations.tcl]

cookit::preInitOS
cookit::initParts
cookit::parseArguments
cookit::initOS
cookit::initAvailablePartVersions

cookit::fixPartVersions

cookit::initDirectories

cookit::partInitializeAllVersions
cookit::runCommand
