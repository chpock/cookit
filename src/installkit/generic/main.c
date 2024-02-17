/*
 * main.c --
 *
 *      Main entry point for wish and other Tk-based applications.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: winMain.c,v 1.15.2.2 2003/12/12 00:42:10 davygrvy Exp $
 */

#include <tcl.h>
#include "crapvfs.h"

#ifdef __WIN32__
#include <tk.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <locale.h>
#endif

#define VFS_MOUNT "/installkitvfs"

extern Tcl_AppInitProc Registry_Init;

/*
 * Forward declarations for procedures defined later in this file:
 */

extern Tcl_PackageInitProc Installkit_Startup;

int _argc;
char **_argv;

#ifdef __WIN32__
static PWCHAR *
wsetargv(int *_argc)
{
    WCHAR   a;
    ULONG   len;
    ULONG   argc;
    ULONG   i, j;
    PWCHAR* argv;
    PWCHAR  _argv;

    BOOLEAN  in_QM;
    BOOLEAN  in_TEXT;
    BOOLEAN  in_SPACE;

    PWCHAR CmdLine = GetCommandLineW();

    len = wcslen(CmdLine);
    i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

    argv = (PWCHAR*)GlobalAlloc(GMEM_FIXED,
        i + (len+2)*sizeof(WCHAR));

    _argv = (PWCHAR)(((PUCHAR)argv)+i);

    argc = 0;
    argv[argc] = _argv;
    in_QM = FALSE;
    in_TEXT = FALSE;
    in_SPACE = TRUE;
    i = 0;
    j = 0;

    while( a = CmdLine[i] ) {
        if(in_QM) {
            if(a == '\"') {
                in_QM = FALSE;
            } else {
                _argv[j] = a;
                j++;
            }
        } else {
            switch(a) {
            case '\"':
                in_QM = TRUE;
                in_TEXT = TRUE;
                if(in_SPACE) {
                    argv[argc] = _argv+j;
                    argc++;
                }
                in_SPACE = FALSE;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                if(in_TEXT) {
                    _argv[j] = '\0';
                    j++;
                }
                in_TEXT = FALSE;
                in_SPACE = TRUE;
                break;
            default:
                in_TEXT = TRUE;
                if(in_SPACE) {
                    argv[argc] = _argv+j;
                    argc++;
                }
                _argv[j] = a;
                j++;
                in_SPACE = FALSE;
                break;
            }
        }
        i++;
    }
    _argv[j] = '\0';
    argv[argc] = NULL;

    (*_argc) = argc;
    return argv;
}
#endif /* __WIN32__ */

/*
 *----------------------------------------------------------------------
 *
 * Installkit_Startup --
 *
 *      This procedure performs application-specific initialization.
 *      Most applications, especially those that incorporate additional
 *      packages, will have their own version of this procedure.
 *
 * Results:
 *      Returns a standard Tcl completion code, and leaves an error
 *      message in the interp's result if an error occurs.
 *
 * Side effects:
 *      Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Installkit_Startup( Tcl_Interp *interp )
{
    int i;
    int res = 0;
    int wrapped = 0;
    Tcl_Obj *wrappedObj, *argvObj = Tcl_NewObj();
#ifdef __WIN32__
    int argc;
    PWCHAR *argv;
    char name[UNICODE_STRING_MAX_CHARS * TCL_UTF_MAX];

    argv = wsetargv(&argc);

    for (i = 0; i < argc; ++i) {
        Tcl_DString ds;
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1,
            name, sizeof(name), NULL, NULL);
        Tcl_UtfToExternalDString(NULL, name, -1, &ds);
        Tcl_ListObjAppendElement(NULL, argvObj,
            Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)));
        Tcl_DStringFree(&ds);
    }
#else
    for (i = 0; i < _argc; ++i) {
        Tcl_DString ds;
        Tcl_UtfToExternalDString(NULL, _argv[i], -1, &ds);
        Tcl_ListObjAppendElement(NULL, argvObj,
            Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)));
        Tcl_DStringFree(&ds);
    }
#endif /* __WIN32__ */

    Tcl_IncrRefCount(argvObj);
    Tcl_SetVar2Ex(interp, "tcl_argv", NULL, argvObj, TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(argvObj);

    /*
     * We have to initialize the virtual filesystem before calling
     * Tcl_Init().  Otherwise, Tcl_Init() will not be able to find
     * its startup script files.
     */

    if( Installkit_Init(interp) == TCL_ERROR ) {
        goto error;
    }

    res = Crapvfs_Mount(interp, (char *)Tcl_GetNameOfExecutable(), VFS_MOUNT);
    if (res == TCL_ERROR) goto error;

    Tcl_SetVar2( interp, "env", "TCLLIBPATH",   VFS_MOUNT "/lib/tcl",
            TCL_GLOBAL_ONLY );
    Tcl_SetVar2( interp, "env", "TCL_LIBRARY",  VFS_MOUNT "/lib/tcl",
            TCL_GLOBAL_ONLY );
    Tcl_SetVar2( interp, "env", "TK_LIBRARY",   VFS_MOUNT "/lib/tk",
            TCL_GLOBAL_ONLY );
    Tcl_SetVar2( interp, "env", "TILE_LIBRARY", VFS_MOUNT "/lib/tile",
            TCL_GLOBAL_ONLY );
    Tcl_SetVar2( interp, "env", "TCL8_6_TM_PATH", VFS_MOUNT "/lib/tcl"
#ifdef __WIN32__
        ";"
#else
        ":"
#endif
        VFS_MOUNT "/lib/tk", TCL_GLOBAL_ONLY );

    if (Tcl_Init(interp) == TCL_ERROR) {
        goto error;
    }

#ifdef __WIN32__
    if( Registry_Init(interp) == TCL_ERROR ) {
        goto error;
    }
    Tcl_StaticPackage(interp, "registry", Registry_Init, NULL);

    if( Tk_Init(interp) == TCL_ERROR ) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage( interp, "tk", Tk_Init, 0 );
#endif /* __WIN32__ */

    Tcl_EvalFile( interp, VFS_MOUNT "/boot.tcl" );

    wrappedObj = Tcl_ObjGetVar2( interp,
            Tcl_NewStringObj( "::installkit::wrapped", -1 ),
            NULL, TCL_GLOBAL_ONLY );

    if( wrappedObj != NULL
            && Tcl_GetIntFromObj( interp, wrappedObj, &wrapped ) != TCL_ERROR
            && wrapped ) {
        Tcl_SetStartupScript(Tcl_NewStringObj(VFS_MOUNT "/main.tcl", -1), NULL);

        if (res == CRAPVFS_MOUNT_PARTIAL) {
            Tcl_Obj *splash = Tcl_NewStringObj(VFS_MOUNT "/splash.tcl", -1);
            Tcl_IncrRefCount(splash);
            Tcl_FSEvalFile(interp, splash);
            Tcl_DecrRefCount(splash);

            Crapvfs_Mount(interp, (char *)Tcl_GetNameOfExecutable(), VFS_MOUNT);
        }
    }

#ifdef __WIN32__
    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */

    if (Tk_CreateConsoleWindow(interp) == TCL_ERROR) {
        goto error;
    }
#endif /* __WIN32__ */

    return TCL_OK;

error:
#ifdef __WIN32__
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, Tcl_GetStringResult(interp), "Fatal Error",
            MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);
#endif /* __WIN32__ */
    return TCL_ERROR;
}


#ifdef __WIN32__
/*
 *-------------------------------------------------------------------------
 *
 * setargv --
 *
 *      Parse the Windows command line string into argc/argv.  Done here
 *      because we don't trust the builtin argument parser in crt0.
 *      Windows applications are responsible for breaking their command
 *      line into arguments.
 *
 *      2N backslashes + quote -> N backslashes + begin quoted string
 *      2N + 1 backslashes + quote -> literal
 *      N backslashes + non-quote -> literal
 *      quote + quote in a quoted string -> single quote
 *      quote + quote not in quoted string -> empty string
 *      quote -> begin quoted string
 *
 * Results:
 *      Fills argcPtr with the number of arguments and argvPtr with the
 *      array of arguments.
 *
 * Side effects:
 *      Memory allocated.
 *
 *--------------------------------------------------------------------------
 */

static void
setargv(argcPtr, argvPtr)
    int *argcPtr;               /* Filled with number of argument strings. */
    char ***argvPtr;            /* Filled with argument strings (malloc'd). */
{
    char *cmdLine, *p, *arg, *argSpace;
    char **argv;
    int argc, size, inquote, copy, slashes;

    cmdLine = GetCommandLine(); /* INTL: BUG */

    /*
     * Precompute an overly pessimistic guess at the number of arguments
     * in the command line by counting non-space spans.
     */

    size = 2;
    for (p = cmdLine; *p != '\0'; p++) {
        if ((*p == ' ') || (*p == '\t')) {      /* INTL: ISO space. */
            size++;
            while ((*p == ' ') || (*p == '\t')) { /* INTL: ISO space. */
                p++;
            }
            if (*p == '\0') {
                break;
            }
        }
    }
    argSpace = (char *) Tcl_Alloc(
            (unsigned) (size * sizeof(char *) + strlen(cmdLine) + 1));
    argv = (char **) argSpace;
    argSpace += size * sizeof(char *);
    size--;

    p = cmdLine;
    for (argc = 0; argc < size; argc++) {
        argv[argc] = arg = argSpace;
        while ((*p == ' ') || (*p == '\t')) {   /* INTL: ISO space. */
            p++;
        }
        if (*p == '\0') {
            break;
        }

        inquote = 0;
        slashes = 0;
        while (1) {
            copy = 1;
            while (*p == '\\') {
                slashes++;
                p++;
            }
            if (*p == '"') {
                if ((slashes & 1) == 0) {
                    copy = 0;
                    if ((inquote) && (p[1] == '"')) {
                        p++;
                        copy = 1;
                    } else {
                        inquote = !inquote;
                    }
                }
                slashes >>= 1;
            }

            while (slashes) {
                *arg = '\\';
                arg++;
                slashes--;
            }

            if ((*p == '\0')
                    || (!inquote && ((*p == ' ') || (*p == '\t')))) {
                /* INTL: ISO space. */
                break;
            }
            if (copy != 0) {
                *arg = *p;
                arg++;
            }
            p++;
        }
        *arg = '\0';
        argSpace = arg + 1;
    }
    argv[argc] = NULL;

    *argcPtr = argc;
    *argvPtr = argv;
}

/*
 *----------------------------------------------------------------------
 *
 * WishPanic --
 *
 *      Display a message and exit.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
WishPanic TCL_VARARGS_DEF(CONST char *,arg1)
{
    va_list argList;
    char buf[1024];
    CONST char *format;

    format = TCL_VARARGS_START(CONST char *,arg1,argList);
    vsprintf(buf, format, argList);

    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, buf, "Fatal Error",
            MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
#ifdef _MSC_VER
    DebugBreak();
#endif
    ExitProcess(1);
}

/*
 *----------------------------------------------------------------------
 *
 * WinMain --
 *
 *      Main entry point from Windows.
 *
 * Results:
 *      Returns false if initialization fails, otherwise it never
 *      returns.
 *
 * Side effects:
 *      Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

int APIENTRY
WinMain(hInstance, hPrevInstance, lpszCmdLine, nCmdShow)
    HINSTANCE hInstance;
    HINSTANCE hPrevInstance;
    LPSTR lpszCmdLine;
    int nCmdShow;
{
    int argc;
    char **argv;

    Tcl_SetPanicProc(WishPanic);

    /*
     * Set up the default locale to be standard "C" locale so parsing
     * is performed correctly.
     */

    setlocale(LC_ALL, "C");
    setargv(&argc, &argv);

    Tk_Main(argc, argv, Installkit_Startup);
    return 1;
}
#else

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *      Main entry point from the console.
 *
 * Results:
 *      None: Tcl_Main never returns here, so this procedure never
 *      returns either.
 *
 * Side effects:
 *      Whatever the applications does.
 *
 *----------------------------------------------------------------------
 */

int
main(int argc, char **argv)
{
    _argc = argc;
    _argv = argv;
    Tcl_Main( argc, argv, Installkit_Startup );
    return TCL_OK;
}
#endif /* __WIN32__ */
