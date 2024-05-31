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
int installkitConsoleMode = 1;

int g_argc = 0;
#ifdef __WIN32__
TCHAR **g_argv;
#else
char **g_argv;
#endif /* __WIN32__ */

static Tcl_Obj *bootstrapObj = NULL;
#ifdef TCL_THREADS
static Tcl_Mutex bootstrapMx;
#endif /* TCL_THREADS */

#define TCL_SCRIPT_HERE(...) #__VA_ARGS__

static char preInitScript[] = TCL_SCRIPT_HERE(
    namespace eval ::installkit {};
    load {} vfs;
    load {} Cookfs;
    proc preinit {} {
        rename preinit {};
        set n [info nameofexecutable];
        if { ![catch {
            if { [info exists bootstrap] } {
                set ::installkit::bootstrap $bootstrap;
                unset bootstrap
            } {
                set ::installkit::cookfspages [::cookfs::c::pages -readonly $n];
                set ::installkit::bootstrap [$::installkit::cookfspages get 0]
            };
            uplevel #0 $::installkit::bootstrap;
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

static char getStartupScript[] = TCL_SCRIPT_HERE(
    if { ![info exists ::argc] || ![info exists ::argv] || !$::argc } { return -code error };
    set ::argv0 [lindex $::argv 0];
    set ::argv [lrange $::argv 1 end];
    incr ::argc -1;
    return $::argv0
);

#ifdef TCL_THREADS
Tcl_AppInitProc Thread_Init;
#endif /* TCL_THREADS */

#ifdef __WIN32__
Tcl_AppInitProc Tk_Init;
#endif /* __WIN32__ */
Tcl_AppInitProc Vfs_Init;
Tcl_AppInitProc Cookfs_Init;
Tcl_AppInitProc Mtls_Init;

#ifdef __WIN32__
Tcl_AppInitProc Registry_Init;
Tcl_AppInitProc Twapi_base_Init;
Tcl_AppInitProc Twapi_Init;
#endif /* __WIN32__ */

void TclX_IdInit (Tcl_Interp *interp);

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
    Tcl_DecrRefCount(exename);

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

    // If the bootstrap code is already retrieved by the first interpreter,
    // it must be used in child interpreters as well.
#ifdef TCL_THREADS
    Tcl_MutexLock(&bootstrapMx);
#endif /* TCL_THREADS */
    if (bootstrapObj != NULL) {
        Tcl_SetVar2Ex(interp, "bootstrap", NULL,
            Tcl_DuplicateObj(bootstrapObj), TCL_GLOBAL_ONLY);
    }
#ifdef TCL_THREADS
    Tcl_MutexUnlock(&bootstrapMx);
#endif /* TCL_THREADS */

    TclSetPreInitScript(preInitScript);

    if (Tcl_Init(interp) != TCL_OK)
        goto error;

    // If we are not in bootstrap mode, run postInit
    if (Tcl_GetVar2Ex(interp, "::installkit::bootstrap_init", NULL,
        TCL_GLOBAL_ONLY) == NULL)
    {

        // Reset the result if the above variable was not found
        Tcl_ResetResult(interp);

        #ifdef TCL_THREADS
        Tcl_MutexLock(&bootstrapMx);
        #endif /* TCL_THREADS */
        // Save bootstrap code. It will be used in child interps.
        if (bootstrapObj == NULL) {
            bootstrapObj = Tcl_GetVar2Ex(interp, "::installkit::bootstrap", NULL,
                TCL_GLOBAL_ONLY);
            if (bootstrapObj == NULL) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("Failed to get"
                    " bootstrap", -1));
                #ifdef TCL_THREADS
                Tcl_MutexUnlock(&bootstrapMx);
                #endif /* TCL_THREADS */
                goto error;
            }
            bootstrapObj = Tcl_DuplicateObj(bootstrapObj);
        }
        #ifdef TCL_THREADS
        Tcl_MutexUnlock(&bootstrapMx);
        #endif /* TCL_THREADS */

        if (Tcl_EvalEx(interp, "::installkit::postInit", -1, TCL_EVAL_GLOBAL) != TCL_OK)
            goto error;

    }

    // Check if we have a wrapped script in VFS
    int isWrappedInt;
    Tcl_Obj *isWrappedObj = Tcl_ObjGetVar2(interp,
        Tcl_NewStringObj("::installkit::wrapped", -1 ),
        NULL, TCL_GLOBAL_ONLY);

    if (isWrappedObj != NULL &&
            Tcl_GetIntFromObj(interp, isWrappedObj, &isWrappedInt) != TCL_ERROR &&
            isWrappedInt) {

        // We have wrapped script in VFS. Use it as startup script.

        Tcl_SetStartupScript(Tcl_NewStringObj(VFS_MOUNT "/main.tcl", -1), NULL);

    } else {

        // Try searching for the startup script on the command line. If it is found,
        // a successful result will be returned.
        if (Tcl_EvalEx(interp, getStartupScript, -1, TCL_EVAL_GLOBAL) == TCL_OK) {
            // We found a startup script, let's use it. It can be retrieved from
            // the script result.
            Tcl_SetStartupScript(Tcl_GetObjResult(interp), NULL);
        } else {
            // No startup script is specified on the command line.
            // Resetting the error state in the interpreter.
            Tcl_ResetResult(interp);
        }

    }

    if (!installkitConsoleMode) {
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
_tmain(int argc, TCHAR *argv[]) {

    g_argc = argc;
    g_argv = argv;

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
main(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
    Tcl_Main(argc, argv, Installkit_Startup);
    return TCL_OK;
}

#endif /* __WIN32__ */
