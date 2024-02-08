package ifneeded installkit 1.1 ";\
    load {} installkit;\
    source [file join $dir installkit.tcl];\
"

package ifneeded installkit::Windows 1.1 ";\
    source [file join $dir wintcl.tcl];\
"

package ifneeded miniarc 0.1 ";\
    installkit::loadMiniarc;\
    source [file join $dir miniarc.tcl];\
"

package ifneeded ttk::theme::jammer 0.1 ";\
    source [file join $dir jammerTheme.tcl];\
"
