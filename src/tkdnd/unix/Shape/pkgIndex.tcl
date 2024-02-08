# Tcl package index file, version 1.0

# This is hand-rolled code.  The name of the shared library is odd, but
# it needs to be mangled like this to get the names working happily on
# broken OSes like SunOS4.  If you have a problem with this, tough!

package ifneeded Shape 0.3 "package require Tk 8\n\
	[list tclPkgSetup $dir Shape 0.3 {{libshape03.so.1.0 load shape}}]"
