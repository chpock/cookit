/*
 * tclXoscmds.c --
 *
 * Tcl commands to access unix system calls that are portable to other
 * platforms.
 *-----------------------------------------------------------------------------
 * Copyright 1991-1999 Karl Lehenbauer and Mark Diekhans.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies.  Karl Lehenbauer and
 * Mark Diekhans make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *-----------------------------------------------------------------------------
 * $Id: tclXoscmds.c,v 1.1 2001/10/24 23:31:48 hobbs Exp $
 *-----------------------------------------------------------------------------
 */

#include "tclExtdInt.h"

static int 
TclX_SystemObjCmd _ANSI_ARGS_((ClientData clientData,
                               Tcl_Interp *interp,
                               int objc,
                               Tcl_Obj *CONST objv[]));


/*-----------------------------------------------------------------------------
 * TclX_SystemObjCmd --
 *   Implements the TCL system command:
 *      system cmdstr1 ?cmdstr2...?
 *-----------------------------------------------------------------------------
 */
static int
TclX_SystemObjCmd (clientData, interp, objc, objv)
    ClientData  clientData;
    Tcl_Interp *interp;
    int         objc;
    Tcl_Obj   *CONST objv[];
{
    Tcl_Obj *cmdObjPtr;
    char *cmdStr;
    int exitCode;

    if (objc < 2)
        return TclX_WrongArgs (interp, objv [0], "cmdstr1 ?cmdstr2...?");

    cmdObjPtr = Tcl_ConcatObj (objc - 1, &(objv[1]));
    cmdStr = Tcl_GetStringFromObj (cmdObjPtr, NULL);

    if (TclXOSsystem (interp, cmdStr, &exitCode) != TCL_OK) {
        Tcl_DecrRefCount (cmdObjPtr);
        return TCL_ERROR;
    }
    Tcl_SetIntObj (Tcl_GetObjResult (interp), exitCode);
    Tcl_DecrRefCount (cmdObjPtr);
    return TCL_OK;
}

/*-----------------------------------------------------------------------------
 * TclX_InitCommands --
 *     Initialize the TclX commands.
 *-----------------------------------------------------------------------------
 */
void
TclX_InitCommands (interp)
    Tcl_Interp *interp;
{
    TclX_CreateObjCommand (interp,
                          "system",
                          TclX_SystemObjCmd,
                          (ClientData) NULL,
                          (Tcl_CmdDeleteProc*) NULL, 0);

}
