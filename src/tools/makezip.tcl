## Usage: makezip.tcl <zipFileName> <file> ?file ...?

package require miniarc
set fp [miniarc::open zip [lindex $argv 0] w]
miniarc::addfiles $fp [lrange $argv 1 end]
miniarc::close $fp
