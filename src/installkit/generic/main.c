/* installkit - main app

 Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version. */

#include <tclInt.h>
#include <tclCookfs.h>

#ifdef TCL_THREADS
#include <tclThread.h>
#endif /* TCL_THREADS */

#ifdef __WIN32__
#include <tk.h>
#include <tkDecls.h>
#define WIN32_LEAN_AND_MEAN
#ifndef STRICT
#define STRICT // See MSDN Article Q83456
#endif /* STRICT */
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
//#include <locale.h>
#include <tchar.h>
#endif

#if defined(__MINGW32__)
int _CRT_glob = 0;
#endif /* __MINGW32__ */

#define VFS_MOUNT "/installkitvfs"

// #define INSTALLKIT_DEBUG

#ifdef INSTALLKIT_DEBUG
#define IkDebug(...) {fprintf(stdout,"[IK DEBUG] "); fprintf(stdout,__VA_ARGS__); fprintf(stdout,"\r\n"); fflush(stdout);}
#else
#define IkDebug(...) {}
#endif

#ifdef __WIN32__
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif /* __WIN32__ */

// 1 - console
// 0 - GUI
int g_isConsoleMode;
int g_isBootstrap;

int g_argc = 0;
#ifdef __WIN32__
TCHAR **g_argv;
#else
char **g_argv;
#endif /* __WIN32__ */

int g_isMainInterp = 1;

#define TCL_SCRIPT_HERE(...) #__VA_ARGS__

static char *preInitScript = "source /installkitvfs/boot.tcl";

static char getStartupScript[] = TCL_SCRIPT_HERE(
    if { ![info exists ::argc] || ![info exists ::argv] || !$::argc } { return -code error };
    set ::argv0 [lindex $::argv 0];
    set ::argv [lrange $::argv 1 end];
    incr ::argc -1;
    return $::argv0
);

#ifdef __WIN32__
Tcl_AppInitProc Tk_Init;
#endif /* __WIN32__ */
Tcl_AppInitProc Vfs_Init;
Tcl_AppInitProc Mtls_Init;

#ifdef __WIN32__
Tcl_AppInitProc Registry_Init;
Tcl_AppInitProc Twapi_base_Init;
Tcl_AppInitProc Twapi_Init;
#endif /* __WIN32__ */

void TclX_IdInit (Tcl_Interp *interp);

static int Installkit_Startup(Tcl_Interp *interp) {

    IkDebug("Installkit_Startup: ENTER to interp: %p", (void *)interp);

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        IkDebug("Installkit_Startup: failed to init stubs");
        return TCL_ERROR;
    }

    // Make sure that we have stdout/stderr/stdin channels. Initialize them
    // to /dev/null if we don't have any. This will prevent Tcl from crashing
    // when attempting to write/read from standard channels. In GUI (Tk) that
    // will be done by Tk_Main()->Tk_InitConsoleChannels().

    if (g_isConsoleMode) {
        Tcl_Channel chan;

        IkDebug("Installkit_Startup: check channels for console mode");
        chan = Tcl_GetStdChannel(TCL_STDIN);
        if (!chan) {
            IkDebug("Installkit_Startup: create stdin");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "r", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDIN);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDOUT);
        if (!chan) {
            IkDebug("Installkit_Startup: create stdout");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "w", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDOUT);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDERR);
        if (!chan) {
            IkDebug("Installkit_Startup: create stderr");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "w", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDERR);
            }
        }
    }

    // Tcl_Main can break $argv by deciding that the user has specified
    // a startup script in it. In this case, Tcl_Main sets $::argv0 to the value
    // it thinks it is, and sets everything else to $::argv. This leads to
    // the fact that we don't know exactly what the command line was. But we
    // need this to properly handle command line arguments.
    //
    // This AI-based behavior of Tcl_Main can be disabled by setting
    // the start script to some fake value. However, in builds with thread
    // support, calling the Tcl_SetStartupScript procedure before Tcl_Main
    // causes a crash.
    //
    // We have no choice but to rebuilt here $::argv $::argc $::argv0 from
    // the original command line.

    // We should not use argv[0] here to get executable name. Let Tcl do
    // it for us. This is especially important on Windows, since argv[0]
    // can contain strange things.
    const char *exenameStr = Tcl_GetNameOfExecutable();
    if (exenameStr == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("failed to get the name"
            " of the current executable file", -1));
        goto error;
    }
    IkDebug("Installkit_Startup: got exe name [%s]", exenameStr);
    Tcl_Obj *exename = Tcl_NewStringObj(exenameStr, -1);
    Tcl_IncrRefCount(exename);
    Tcl_SetVar2Ex(interp, "argv0", NULL, exename, TCL_GLOBAL_ONLY);

    // Skip argv[0], since we don't need it anymore.
    if (g_argc) {
        g_argc--;
        g_argv++;
    }

    Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewIntObj(g_argc),
        TCL_GLOBAL_ONLY);

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    Tcl_Obj *argvObj = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(argvObj);
    while (g_argc--) {
        IkDebug("Installkit_Startup: add arg...");
#ifdef UNICODE
        Tcl_WinTCharToUtf(*g_argv++, -1, &ds);
#else
        Tcl_ExternalToUtfDString(NULL, (char *)*g_argv++, -1, &ds);
#endif /* UNICODE */
        Tcl_ListObjAppendElement(interp, argvObj,
            Tcl_NewStringObj(Tcl_DStringValue(&ds), -1));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, argvObj, TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(argvObj);
    Tcl_DStringFree(&ds);

    // Reset the start script, from a possible AI-based detection in Tcl_Main.
    // If necessary, we will set the desired value later.
    Tcl_SetStartupScript(NULL, NULL);

    // Init TclX
    TclX_IdInit(interp);

    IkDebug("Installkit_Startup: register static packages");
    Tcl_StaticPackage(0, "vfs", Vfs_Init, NULL);
    Tcl_StaticPackage(0, "Cookfs", Cookfs_Init, NULL);
    Tcl_StaticPackage(0, "mtls", Mtls_Init, NULL);

#ifdef __WIN32__
    Tcl_StaticPackage(0, "Tk", Tk_Init, NULL);
#endif /* __WIN32__ */

#ifdef TCL_THREADS
    Tcl_StaticPackage(0, "Thread", Thread_Init, NULL);
#endif /* TCL_THREADS */

#ifdef __WIN32__
    Tcl_StaticPackage(0, "registry", Registry_Init, NULL);
    Tcl_StaticPackage(0, "twapi_base", Twapi_base_Init, NULL);
    Tcl_StaticPackage(0, "twapi", Twapi_Init, NULL);
#endif /* __WIN32__ */

    Tcl_CreateNamespace(interp, "::installkit", NULL, NULL);

    Tcl_SetVar2Ex(interp, "::installkit::main_interp", NULL,
        Tcl_NewIntObj(g_isMainInterp), TCL_GLOBAL_ONLY);
    if (g_isMainInterp) {
        IkDebug("Installkit_Startup: is main interp");
        g_isMainInterp = 0;
    } else {
        IkDebug("Installkit_Startup: is slave interp");
    }

    Tcl_Obj *local = Tcl_NewStringObj(VFS_MOUNT, -1);
    Tcl_IncrRefCount(local);

    Tcl_SetVar2Ex(interp, "::installkit::root", NULL, local, TCL_GLOBAL_ONLY);

    IkDebug("Installkit_Startup: init cookfs");
    if (Cookfs_Init(interp) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("failed to init cookfs"
            " package", -1));
        IkDebug("Installkit_Startup: ERROR");
        goto error;
    }

    void *props = Cookfs_VfsPropsInit();
    Cookfs_VfsPropSetVolume(props, 1);
    Cookfs_VfsPropSetReadonly(props, 1);
#ifdef TCL_THREADS
    Cookfs_VfsPropSetShared(props, 1);
#endif /* TCL_THREADS */
    IkDebug("Installkit_Startup: mount root vfs");
    int isVFSAvailable = Cookfs_Mount(interp, exename, local, props) == TCL_OK
        ? 1 : 0;
    IkDebug("Installkit_Startup: vfs available: %d", isVFSAvailable);

    Cookfs_VfsPropsFree(props);

    Tcl_DecrRefCount(exename);
    Tcl_DecrRefCount(local);

    if (isVFSAvailable) {
        // If VFS available, then use boot script from it
        //if (Tcl_EvalFile(interp, VFS_MOUNT "/boot.tcl") != TCL_OK) {
        //    goto error;
        //}
        IkDebug("Installkit_Startup: set boot.tcl as a pre-init script");
        TclSetPreInitScript(preInitScript);
    } else if (!g_isBootstrap) {
        IkDebug("Installkit_Startup: FATAL! vfs is not available");
        // If VFS unavailable and we are not in VFS bootstrap, throw an error.
        goto error;
    }

    IkDebug("Installkit_Startup: initialize interp...");
    if (Tcl_Init(interp) != TCL_OK) {
        goto error;
    }

    // ::installkit::postInit is from boot.tcl and is only available when
    // VFS is present
    if (isVFSAvailable) {
        IkDebug("Installkit_Startup: call postInit ...");
        if (Tcl_EvalEx(interp, "::installkit::postInit", -1, TCL_EVAL_GLOBAL
            | TCL_EVAL_DIRECT) != TCL_OK)
        {
            goto error;
        }
    }

    // Check if we have a wrapped script in VFS
    int isWrappedInt;
    Tcl_Obj *isWrappedObj = Tcl_ObjGetVar2(interp,
        Tcl_NewStringObj("::installkit::wrapped", -1 ), NULL, TCL_GLOBAL_ONLY);

    if (isWrappedObj != NULL &&
            Tcl_GetIntFromObj(interp, isWrappedObj, &isWrappedInt) != TCL_ERROR
            && isWrappedInt)
    {

        // We have wrapped script in VFS. Use it as startup script.
        IkDebug("Installkit_Startup: have wrapped script, use main.tcl");

        Tcl_SetStartupScript(Tcl_NewStringObj(VFS_MOUNT "/main.tcl", -1), NULL);

    } else {

        IkDebug("Installkit_Startup: not wrapped");
        // Try searching for the startup script on the command line. If it is
        // found, a successful result will be returned.
        if (Tcl_EvalEx(interp, getStartupScript, -1,
            TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK)
        {
            // We found a startup script, let's use it. It can be retrieved from
            // the script result.
            Tcl_SetStartupScript(Tcl_GetObjResult(interp), NULL);
            IkDebug("Installkit_Startup: found startup script in cmd line args");
        } else {
            // No startup script is specified on the command line.
            // Resetting the error state in the interpreter.
            Tcl_ResetResult(interp);
            IkDebug("Installkit_Startup: no startup script in cmd line args");
        }

    }

    if (!g_isConsoleMode) {
        IkDebug("Installkit_Startup: init GUI...");
#ifdef __WIN32__
        if (Tk_Init(interp) != TCL_OK)
            goto error;
        // Create console if we run without script and in GUI mode.
        if (Tk_CreateConsoleWindow(interp) != TCL_OK)
            goto error;
#else
        if (Tcl_EvalEx(interp, "package require Tk", -1, TCL_EVAL_GLOBAL) != TCL_OK)
            goto error;
#endif /* __WIN32__ */
    }

    IkDebug("Installkit_Startup: ok");
    return TCL_OK;

error:
    IkDebug("Installkit_Startup: RETURN ERROR");
#ifdef __WIN32__
    fprintf(stderr, "Fatal error: %s\n", Tcl_GetStringResult(interp));
    fflush(stderr);
    if (!g_isConsoleMode) {
        MessageBeep(MB_ICONEXCLAMATION);
        MessageBoxA(NULL, Tcl_GetStringResult(interp), "Fatal error",
            MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    }
    ExitProcess(1);
#endif /* __WIN32__ */
    return TCL_ERROR;
}

#ifdef __WIN32__

int
_tmain(int argc, TCHAR *argv[]) {

    g_argc = argc;
    g_argv = argv;

    g_isConsoleMode = GetEnvironmentVariableA("INSTALLKIT_CONSOLE",
        NULL, 0) == 0 ? 0 : 1;
    g_isBootstrap = GetEnvironmentVariableA("INSTALLKIT_BOOTSTRAP",
        NULL, 0) == 0 ? 0 : 1;

    if (g_isConsoleMode) {
        Tcl_Main(argc, argv, Installkit_Startup);
    } else {
        Tk_Main(argc, argv, Installkit_Startup);
    }

    return TCL_OK;

}

#else

int
main(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;

    g_isConsoleMode = getenv("INSTALLKIT_GUI") == NULL ? 1 : 0;
    g_isBootstrap = getenv("INSTALLKIT_BOOTSTRAP") == NULL ? 0 : 1;

    Tcl_Main(argc, argv, Installkit_Startup);
    return TCL_OK;
}

#endif /* __WIN32__ */
