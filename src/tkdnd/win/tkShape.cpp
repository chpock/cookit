/*
 * tkShape.cpp
 *  This file implements the "shape" command under windows. As currently there
 *  is not such an implementation, this file tries to provide a non-working
 *  version of it, in order to not break scripts that use the "shape" command
 *  under windows.
 *  TODO: Provide the needed functionality :-)
 */
#include <tcl.h>

static int tkDND_ShapeObjCmd(ClientData clientData, Tcl_Interp *interp,
                             int objc, Tcl_Obj *const objv[]) {
  Tcl_ResetResult(interp);
  if (objc<2) {
    Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?window arg ...?");
    return TCL_ERROR;
  }
  return TCL_OK;
} /* tkShapeObjCmd */

int Shape_Init(Tcl_Interp *interp) {
  Tcl_CreateObjCommand(interp, "shape", tkDND_ShapeObjCmd, NULL, NULL);
  Tcl_SetVar(interp, "shape_version",    "0.3",   TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "shape_patchLevel", "0.3a1", TCL_GLOBAL_ONLY);
  return Tcl_PkgProvide(interp, "shape", "0.3");
} /* Shape_Init */

/*
 * EOF
 */
