/* 
 * tclAppInit.c --
 *
 *  Provides a default version of the main program and Tcl_AppInit
 *  procedure for Tcl applications (without Tk).  Note that this
 *  program must be built in Win32 console mode to work properly.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 2000-2002 Jean-Claude Wippler <jcw@equi4.com>
 * Copyright (c) 2003-2004 Wojciech Kocjan <wojciech.kocjan@dq.pl>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef MAC_TCL

/* %COOKIT_INCLUDE_CODE% */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

#else

/* preprocessor hack to allow us to use our own main below */
#define main main_unused
#include "tkMacAppInit.c"
#undef main
#endif

#include "tclInt.h"

/* %COOKIT_EXTERN_CODE% */
Tcl_AppInitProc        Vfs_Init;
Tcl_AppInitProc        Cookfs_Init;

/* OLD CODE 
static char preInitCmd[] =
"proc getCookfsBootstrap {f} {set h [open $f r];fconfigure $h -translation binary;seek $h -32 end;set d [read $h 32];if {[binary scan $d WIIIIca7 o - - s - c S] != 7} {error {Invalid data}};if {$S=={CFS0001}} {seek $h $o start;set bs [read $h $s];if {$c==0} {} elseif {$c==1} {if {[catch {set bs [zlib decompress $bs]}]} {package require Trf;set bs [zip -mode decompress -- $bs]}} else {error {Invalid compression}}} else {error {Invalid Signature}};return $bs}\n"
"uplevel #0 [getCookfsBootstrap [info nameofexecutable]]\n"
"rename getCookfsBootstrap {}\n"
"";
*/

static char preInitCmd[] =
"namespace eval ::cookit {}\n"
"load {} Cookfs\n"
"load {} vfs\n"
"proc getCookfsBootstrap {} {\n"
#ifdef COOKIT_STATIC_ZLIB
"load {} zlib\n"
#endif
" set ::cookit::cookitpages [cookfs::pages -readonly [info nameofexecutable]]\n"
" uplevel #0 [$::cookit::cookitpages get 0]\n"
"}\n"
"if {[catch {\n"
" getCookfsBootstrap ; rename getCookfsBootstrap {}\n"
"}]} {\n"
" set errorMsg \"Initialization error\"                                            \n"
#ifdef _WIN32
#ifdef KIT_INCLUDES_TK
" catch {\n"
"  wm withdraw .\n"
"  tk_messageBox -message $errorMsg -title \"Fatal Error\"\n"
"  exit 127\n"
" }\n"
#endif
#endif
" puts stderr $errorMsg\n"
" exit 127\n"
"}\n"
"";

int 
CooKit_AppInit(Tcl_Interp *interp)
{
    char *oldCmd;
    char *newInitScript;
    int rc;
    
    Tcl_StaticPackage(0, "Cookfs", Cookfs_Init, NULL);
    Tcl_StaticPackage(0, "vfs", Vfs_Init, NULL);
/* %COOKIT_APPINIT_CODE% */

#ifdef _WIN32
#ifdef KIT_INCLUDES_TK
    Tk_InitConsoleChannels(interp);
#endif
    Tcl_SetVar(interp, "tcl_rcFileName", "~/cookitrc.tcl", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.cookitrc", TCL_GLOBAL_ONLY);
#ifdef MAC_TCL
    Tcl_SetVar(interp, "tcl_rcRsrcName", "cookitrc", TCL_GLOBAL_ONLY);
#endif
#endif

    /* initialize first interpreter with code read from bootstrap of CookFS */
    TclSetPreInitScript(preInitCmd);
    if (Tcl_Init(interp) == TCL_ERROR)
        goto error;

    /* initialize arg* based on whether main.tcl exists in VFS */
    newInitScript = strdup("_cookit_handlemainscript");
    rc = Tcl_EvalEx(interp, newInitScript, -1, TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL);
    free(newInitScript);

    if (rc == TCL_ERROR) {
        goto error;
    }
    else if (rc == TCL_OK) {
        Tcl_Obj *result = Tcl_GetObjResult(interp);
        int resultLen = 0;

        if (result != NULL)
            Tcl_GetStringFromObj(result, &resultLen);

        if (resultLen > 0) {
            const char *encoding = NULL;
            Tcl_Obj* path = Tcl_GetStartupScript(&encoding);
            Tcl_SetStartupScript(result, encoding);
            if (path == NULL) {
                Tcl_Eval(interp, "incr argc -1; set argv [lrange $argv 1 end]");
            }
        }
    }

    /* get init script that should be used for all subsequent interpreter creations - it will be different from one for first interpreter */
    newInitScript = strdup("_cookit_dumpinitcode");
    rc = Tcl_EvalEx(interp, newInitScript, -1, TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL);
    free(newInitScript);

    if (rc == TCL_ERROR)
        goto error;

    newInitScript = strdup(Tcl_GetStringResult(interp));
    TclSetPreInitScript(newInitScript);

    /* move on with usual initialization stuff */
#ifdef KIT_INCLUDES_TK
#if defined(_WIN32) || defined(MAC_TCL)
    if (Tk_Init(interp) == TCL_ERROR)
    {
        goto error;
    }
#ifdef _WIN32
    if (Tk_CreateConsoleWindow(interp) == TCL_ERROR)
    {
        goto error;
    }
#else
    SetupMainInterp(interp);
#endif

#endif
#endif

    Tcl_SetVar(interp, "errorInfo", "", TCL_GLOBAL_ONLY);
    Tcl_ResetResult(interp);
    return TCL_OK;

error:
#ifdef KIT_INCLUDES_TK
#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, Tcl_GetStringResult(interp), "Error in CooKit",
        MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);
    /* we won't reach this, but we need the return */
#endif
#endif
    return TCL_ERROR;
}

#ifdef MAC_TCL
void
main( int argc, char **argv)
{
    char *newArgv[2];

    if (MacintoshInit()  != TCL_OK) {
    Tcl_Exit(1);
    }

    argc = 1;
    newArgv[0] = "CooKit";
    newArgv[1] = NULL;

    Tcl_FindExecutable(newArgv[0]);

    Tk_Main(argc, newArgv, CooKit_AppInit);
}
#endif
