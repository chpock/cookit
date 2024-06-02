/* installkit - main app

 Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version. */

#include <tclInt.h>

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

static char getStartupScript[] = TCL_SCRIPT_HERE(
    if { ![info exists ::argc] || ![info exists ::argv] || !$::argc } { return -code error };
    set ::argv0 [lindex $::argv 0];
    set ::argv [lrange $::argv 1 end];
    incr ::argc -1;
    return $::argv0
);

Tcl_AppInitProc Cookfs_Init;

int Cookfs_Mount(Tcl_Interp *interp, Tcl_Obj *archive, Tcl_Obj *local,
    void *props);
void *Cookfs_VfsPropsInit(void *p);
void Cookfs_VfsPropsFree(void *p);
void Cookfs_VfsPropSetVolume(void *p, int volume);
void Cookfs_VfsPropSetReadonly(void *p, int readonly);

#ifdef TCL_THREADS
Tcl_AppInitProc Thread_Init;
#endif /* TCL_THREADS */

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

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    // Make sure that we have stdout/stderr/stdin channels. Initialize them
    // to /dev/null if we don't have any. This will prevent Tcl from crashing
    // when attempting to write/read from standard channels. In GUI (Tk) that
    // will be done by Tk_Main()->Tk_InitConsoleChannels().

    if (g_isConsoleMode) {
        Tcl_Channel chan;

        chan = Tcl_GetStdChannel(TCL_STDIN);
        if (!chan) {
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "r", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDIN);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDOUT);
        if (!chan) {
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "w", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDOUT);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDERR);
        if (!chan) {
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


    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    // The first argument to argv is the name of the executable file. Set its
    // value to $::argv0. However, before any manipulation with argv, we should
    // make sure that argc is not zero.

    Tcl_Obj *exename;

    if (g_argc) {
#ifdef UNICODE
        Tcl_WinTCharToUtf(*g_argv++, -1, &ds);
#else
        Tcl_ExternalToUtfDString(NULL, (char *)*g_argv++, -1, &ds);
#endif /* UNICODE */
        g_argc--;
        exename = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
    } else {
        exename = Tcl_NewStringObj(NULL, 0);
    }
    Tcl_IncrRefCount(exename);
    Tcl_SetVar2Ex(interp, "argv0", NULL, exename, TCL_GLOBAL_ONLY);

    Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewIntObj(g_argc),
        TCL_GLOBAL_ONLY);

    Tcl_Obj *argvObj = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(argvObj);
    while (g_argc--) {
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

    // Reset the start script, from a possible AI-based detection in Tcl_Main.
    // If necessary, we will set the desired value later.
    Tcl_SetStartupScript(NULL, NULL);

    // Init TclX
    TclX_IdInit(interp);

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
        g_isMainInterp = 0;
    }

    Tcl_Obj *local = Tcl_NewStringObj(VFS_MOUNT, -1);
    Tcl_IncrRefCount(local);

    Tcl_SetVar2Ex(interp, "::installkit::root", NULL, local, TCL_GLOBAL_ONLY);

    if (Cookfs_Init(interp) != TCL_OK) {
        goto error;
    }

    void *props = Cookfs_VfsPropsInit(NULL);
    Cookfs_VfsPropSetVolume(props, 1);
    Cookfs_VfsPropSetReadonly(props, 1);
    int isVFSAvailable = Cookfs_Mount(interp, exename, local, props) == TCL_OK
        ? 1 : 0;

    Cookfs_VfsPropsFree(props);

    Tcl_DecrRefCount(exename);
    Tcl_DecrRefCount(local);

    if (isVFSAvailable) {
        // If VFS available, then use boot script from it
        if (Tcl_EvalFile(interp, VFS_MOUNT "/boot.tcl") != TCL_OK) {
            goto error;
        }
    } else if (!g_isBootstrap) {
        // If VFS unavailable and we are not in VFS bootstrap, throw an error.
        goto error;
    }

    if (Tcl_Init(interp) != TCL_OK) {
        goto error;
    }

    // ::installkit::postInit is from boot.tcl and is only available when
    // VFS is present
    if (isVFSAvailable) {
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

        Tcl_SetStartupScript(Tcl_NewStringObj(VFS_MOUNT "/main.tcl", -1), NULL);

    } else {

        // Try searching for the startup script on the command line. If it is
        // found, a successful result will be returned.
        if (Tcl_EvalEx(interp, getStartupScript, -1,
            TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT) == TCL_OK)
        {
            // We found a startup script, let's use it. It can be retrieved from
            // the script result.
            Tcl_SetStartupScript(Tcl_GetObjResult(interp), NULL);
        } else {
            // No startup script is specified on the command line.
            // Resetting the error state in the interpreter.
            Tcl_ResetResult(interp);
        }

    }

    if (!g_isConsoleMode) {
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

    return TCL_OK;

error:
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
