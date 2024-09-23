/* cookit - package

 Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>

 See the file "license.terms" for information on usage and redistribution of
 this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

#include "cookit.h"
#include <unistd.h> // for isatty()

static Tcl_Config const cookit_pkgconfig[] = {
    { "package-version",  PACKAGE_VERSION },
    { "platform",         COOKIT_PLATFORM },
    {NULL, NULL}
};

static int cookit_IsTtyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    (void)clientData;

    static const struct {
        const char *name;
        int fd;
    } fd_names[] = {
        { "stdin",  0 },
        { "stdout", 1 },
        { "stderr", 2 },
        { NULL }
    };

    int fd = 1;

    if (objc > 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?file_descriptor?");
        return TCL_ERROR;
    }

    if (objc == 2) {

        if (Tcl_GetIntFromObj(NULL, objv[1], &fd) == TCL_OK) {
            if (fd < 0 || fd > 2) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("file descriptor should be"
                    " from 0 to 2, but got \"%s\"", Tcl_GetString(objv[1])));
                return TCL_ERROR;
            }
        } else {
            int idx;
            if (Tcl_GetIndexFromObjStruct(interp, objv[1], fd_names, sizeof(fd_names[0]), "file descriptio", 0, &idx) != TCL_OK) {
                return TCL_ERROR;
            }
            fd = fd_names[idx].fd;
        }

    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(isatty(fd)));
    return TCL_OK;

}


#if TCL_MAJOR_VERSION > 8
#define MIN_TCL_VERSION "9.0"
#else
#define MIN_TCL_VERSION "8.6"
#endif

DLLEXPORT int Cookit_Init(Tcl_Interp *interp)  {

    if (Tcl_InitStubs(interp, MIN_TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "::cookit::is_tty", cookit_IsTtyCmd, NULL, NULL);

    Tcl_RegisterConfig(interp, PACKAGE_NAME, cookit_pkgconfig, "iso8859-1");

    return TCL_OK;

}
