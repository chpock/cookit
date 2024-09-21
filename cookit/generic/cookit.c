/* cookit - package

 Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>

 See the file "license.terms" for information on usage and redistribution of
 this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

#include "cookit.h"

static Tcl_Config const cookit_pkgconfig[] = {
    { "package-version",  PACKAGE_VERSION },
    { "platform",         COOKIT_PLATFORM },
    {NULL, NULL}
};


#if TCL_MAJOR_VERSION > 8
#define MIN_TCL_VERSION "9.0"
#else
#define MIN_TCL_VERSION "8.6"
#endif

DLLEXPORT int Cookit_Init(Tcl_Interp *interp)  {

    if (Tcl_InitStubs(interp, MIN_TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    Tcl_RegisterConfig(interp, PACKAGE_NAME, cookit_pkgconfig, "iso8859-1");

    return TCL_OK;

}
