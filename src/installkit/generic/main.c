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
#endif

// 1 - console
// 0 - GUI
int installkitConsoleMode = 1;

#define TCL_SCRIPT_HERE(...) #__VA_ARGS__

static char preInitScript[] = TCL_SCRIPT_HERE(
    namespace eval ::installkit {};
    load {} vfs;
    load {} Cookfs;
    proc preinit {} {
        rename preinit {};
        set n [info nameofexecutable];
        if { ![catch {
            set ::installkit::cookfspages [::cookfs::c::pages -readonly $n];
            uplevel #0 [$::installkit::cookfspages get 0];
            ::installkit::preInit
        } e] } { return };
        if { [info exists ::env(INSTALLKIT_BOOTSTRAP)] } {
            set ::installkit::bootstrap_init 1;
            return
        };
        set f [open $n r];
        fconfigure $f -encoding binary -translation binary;
        seek $f -17 end;
        set s [read $f 17];
        close $f;
        binary scan $s a7a2W a b c;
        if { $a ne {IKMAGIC} || $b ne {IK} } {
            return -code error "$e; Magic stamp not found."
        };
        set s [file size $n];
        if { $c == $s } {
            return -code error "The executable file is corrupted."
        } {
            return -code error "The executable file is corrupted. Expected file size: $c; Actual file size: $n"
        }
    };
    preinit
);

#ifdef TCL_THREADS
Tcl_AppInitProc Thread_Init;
#endif /* TCL_THREADS */

#ifdef __WIN32__
Tcl_AppInitProc Tk_Init;
#endif /* __WIN32__ */
Tcl_AppInitProc Vfs_Init;
Tcl_AppInitProc Cookfs_Init;

#ifdef __WIN32__
Tcl_AppInitProc Registry_Init;
Tcl_AppInitProc Twapi_base_Init;
Tcl_AppInitProc Twapi_Init;
#endif /* __WIN32__ */

int
Installkit_Startup(Tcl_Interp *interp) {

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    // Make sure that we have stdout/stderr/stdin channels. Initialize them
    // to /dev/null if we don't have any. This will prevent Tcl from crashing
    // when attempting to write/read from standard channels. In GUI (Tk) that
    // will be done by Tk_Main()->Tk_InitConsoleChannels().

    if (installkitConsoleMode) {
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

    Tcl_StaticPackage(0, "vfs", Vfs_Init, NULL);

    if (Vfs_Init(interp) != TCL_OK)
        goto error;

    Tcl_StaticPackage(0, "Cookfs", Cookfs_Init, NULL);

    if (Cookfs_Init(interp) != TCL_OK)
        goto error;

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

    TclSetPreInitScript(preInitScript);

    if (Tcl_Init(interp) != TCL_OK)
        goto error;

    // if we are in bootstrap mode
    if (Tcl_GetVar2Ex(interp, "::installkit::bootstrap_init", NULL, TCL_GLOBAL_ONLY) != NULL)
        return TCL_OK;
    // reset the result if the above variable was not found
    Tcl_ResetResult(interp);

    if (Tcl_EvalEx(interp, "::installkit::postInit", -1, TCL_EVAL_GLOBAL) != TCL_OK)
        goto error;

    if (!installkitConsoleMode) {
#ifdef __WIN32__
        if (Tk_Init(interp) != TCL_OK)
#else
        if (Tcl_EvalEx(interp, "package require Tk", -1, TCL_EVAL_GLOBAL) != TCL_OK)
#endif /* __WIN32__ */
            goto error;
    }

    // source the main script here if wrapped
    int isWrappedInt;
    Tcl_Obj *isWrappedObj = Tcl_ObjGetVar2(interp,
        Tcl_NewStringObj("::installkit::wrapped", -1 ),
        NULL, TCL_GLOBAL_ONLY);

    if (isWrappedObj != NULL &&
            Tcl_GetIntFromObj(interp, isWrappedObj, &isWrappedInt) != TCL_ERROR &&
            isWrappedInt) {

        Tcl_SetStartupScript(Tcl_NewStringObj(VFS_MOUNT "/main.tcl", -1), NULL);

    } else if (!installkitConsoleMode) {

#ifdef __WIN32__
        // create console if we run without script and in GUI mode
        if (Tk_CreateConsoleWindow(interp) != TCL_OK)
            goto error;
#endif /* __WIN32__ */

    }

    return TCL_OK;

error:
#ifdef __WIN32__
    fprintf(stderr, "Fatal error: %s\n", Tcl_GetStringResult(interp));
    fflush(stderr);
    if (!installkitConsoleMode) {
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
_tmain(int argc, TCHAR *argv[])
{

    installkitConsoleMode = GetEnvironmentVariableA("INSTALLKIT_CONSOLE",
        NULL, 0) == 0 ? 0 : 1;

    if (installkitConsoleMode) {
        Tcl_Main(argc, argv, Installkit_Startup);
    } else {
        Tk_Main(argc, argv, Installkit_Startup);
    }

    return TCL_OK;
}

#else

int
main(int argc, char **argv)
{
    Tcl_Main(argc, argv, Installkit_Startup);
    return TCL_OK;
}

#endif /* __WIN32__ */
