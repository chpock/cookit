/* cookit - main app

 Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>

 See the file "license.terms" for information on usage and redistribution of
 this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

#include <tclInt.h>
#include <tclCookfs.h>
#include "cookit.h"

#ifdef TCL_THREADS
#include <tclThread.h>
#endif /* TCL_THREADS */

#ifdef __WIN32__
#ifndef COOKIT_CONSOLE_ONLY
#include <tk.h>
#include <tkDecls.h>
#endif /* COOKIT_CONSOLE_ONLY */
#define WIN32_LEAN_AND_MEAN
#ifndef STRICT
#define STRICT // See MSDN Article Q83456
#endif /* STRICT */
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
//#include <locale.h>
#include <tchar.h>
#endif

#ifndef TCL_SIZE_MAX
#ifndef Tcl_Size
typedef int Tcl_Size;
#endif /* Tcl_Size */
#define TCL_SIZE_MAX INT_MAX
#endif /* TCL_SIZE_MAX */

#if defined(__MINGW32__)
int _CRT_glob = 0;
#endif /* __MINGW32__ */

#define VFS_MOUNT "//cookit:/"

// #define COOKIT_DEBUG

#ifdef COOKIT_DEBUG
#define DBG(...) {fprintf(stdout,"[DEBUG] "); fprintf(stdout,__VA_ARGS__); fprintf(stdout,"\r\n"); fflush(stdout);}
#else
#define DBG(...) {}
#endif

#ifdef __WIN32__
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif /* __WIN32__ */

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

#ifndef COOKIT_CONSOLE_ONLY
// 1 - console
// 0 - GUI
int g_isConsoleMode;
#endif /* COOKIT_CONSOLE_ONLY */
int g_isBootstrap;

int g_argc = 0;
#ifdef __WIN32__
TCHAR **g_argv;
#else
char **g_argv;
#endif /* __WIN32__ */

#ifdef __WIN32__
#ifndef COOKIT_CONSOLE_ONLY
Tcl_AppInitProc Tk_Init;
#endif /* COOKIT_CONSOLE_ONLY */
#endif /* __WIN32__ */
Tcl_AppInitProc Vfs_Init;
Tcl_AppInitProc Mtls_Init;
Tcl_AppInitProc Tdom_Init;

#ifdef __WIN32__
Tcl_AppInitProc Registry_Init;
Tcl_AppInitProc Twapi_base_Init;
#endif /* __WIN32__ */

void TclX_IdInit (Tcl_Interp *interp);

static int Cookit_Startup(Tcl_Interp *interp) {

    DBG("Cookit_Startup: ENTER to interp: %p", (void *)interp);

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        DBG("Cookit_Startup: failed to init stubs");
        return TCL_ERROR;
    }

    // Make sure that we have stdout/stderr/stdin channels. Initialize them
    // to /dev/null if we don't have any. This will prevent Tcl from crashing
    // when attempting to write/read from standard channels. In GUI (Tk) that
    // will be done by Tk_Main()->Tk_InitConsoleChannels().

#ifndef COOKIT_CONSOLE_ONLY
    if (g_isConsoleMode) {
#endif /* COOKIT_CONSOLE_ONLY */
        Tcl_Channel chan;

        DBG("Cookit_Startup: check channels for console mode");
        chan = Tcl_GetStdChannel(TCL_STDIN);
        if (!chan) {
            DBG("Cookit_Startup: create stdin");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "r", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDIN);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDOUT);
        if (!chan) {
            DBG("Cookit_Startup: create stdout");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "w", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDOUT);
            }
        }

        chan = Tcl_GetStdChannel(TCL_STDERR);
        if (!chan) {
            DBG("Cookit_Startup: create stderr");
            chan = Tcl_OpenFileChannel(interp, NULL_DEVICE, "w", 0);
            if (chan) {
                Tcl_SetChannelOption(interp, chan, "-encoding", "utf-8");
                Tcl_SetStdChannel(chan, TCL_STDERR);
            }
        }
#ifndef COOKIT_CONSOLE_ONLY
    }
#endif /* COOKIT_CONSOLE_ONLY */

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
    DBG("Cookit_Startup: got exe name [%s]", exenameStr);
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
    int argvLength = 0;
    while (g_argc--) {
        DBG("Cookit_Startup: add arg...");
#ifdef UNICODE
#if (TCL_MAJOR_VERSION < 9) && defined(TCL_MINOR_VERSION) && (TCL_MINOR_VERSION < 7)
        Tcl_WinTCharToUtf(*g_argv++, -1, &ds);
#else
        Tcl_DStringSetLength(&ds, 0);
        Tcl_WCharToUtfDString(*g_argv++, -1, &ds);
#endif
#else
        Tcl_ExternalToUtfDString(NULL, (char *)*g_argv++, -1, &ds);
#endif /* UNICODE */
        DBG("Add arg [%s]", Tcl_DStringValue(&ds));
        Tcl_ListObjAppendElement(interp, argvObj,
            Tcl_NewStringObj(Tcl_DStringValue(&ds), -1));
        argvLength++;
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, argvObj, TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&ds);

    // Reset the start script, from a possible AI-based detection in Tcl_Main.
    // If necessary, we will set the desired value later.
    Tcl_SetStartupScript(NULL, NULL);

    // Init TclX
    TclX_IdInit(interp);

    DBG("Cookit_Startup: register static packages");
    Tcl_StaticPackage(0, "Cookit", Cookit_Init, NULL);
    Tcl_StaticPackage(0, "Vfs", Vfs_Init, NULL);
    Tcl_StaticPackage(0, "Cookfs", Cookfs_Init, NULL);
    Tcl_StaticPackage(0, "Mtls", Mtls_Init, NULL);
    Tcl_StaticPackage(0, "Tdom", Tdom_Init, NULL);

#ifdef __WIN32__
#ifndef COOKIT_CONSOLE_ONLY
    Tcl_StaticPackage(0, "Tk", Tk_Init, NULL);
#endif /* COOKIT_CONSOLE_ONLY */
#endif /* __WIN32__ */

#ifdef TCL_THREADS
    Tcl_StaticPackage(0, "Thread", Thread_Init, NULL);
#endif /* TCL_THREADS */

#ifdef __WIN32__
    Tcl_StaticPackage(0, "Registry", Registry_Init, NULL);
    Tcl_StaticPackage(0, "Twapi_base", Twapi_base_Init, NULL);
#endif /* __WIN32__ */

    Tcl_Obj *local = Tcl_NewStringObj(VFS_MOUNT, -1);
    Tcl_IncrRefCount(local);

    Tcl_CreateNamespace(interp, "::cookit", NULL, NULL);
    Tcl_SetVar2Ex(interp, "::cookit::root", NULL, local, TCL_GLOBAL_ONLY);

    DBG("Cookit_Startup: init cookfs");
    if (Cookfs_Init(interp) != TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("failed to init cookfs"
            " package", -1));
        DBG("Cookit_Startup: ERROR");
        goto error;
    }

    void *props = Cookfs_VfsPropsInit();
    Cookfs_VfsPropSetVolume(props, 1);
    Cookfs_VfsPropSetReadonly(props, 1);
#ifdef TCL_THREADS
    Cookfs_VfsPropSetShared(props, 1);
#endif /* TCL_THREADS */
    DBG("Cookit_Startup: mount root vfs");
    int isVFSAvailable = Cookfs_Mount(interp, exename, local, props) == TCL_OK
        ? 1 : 0;
    DBG("Cookit_Startup: vfs available: %d", isVFSAvailable);

    Cookfs_VfsPropsFree(props);

    Tcl_DecrRefCount(exename);
    Tcl_DecrRefCount(local);

    if (isVFSAvailable) {

        // If VFS is available, then configure environment variables to load
        // Tcl runtime/packages from it.
        // See also: ./src/tcl-020-All-do-not-use-exe-directory.patch

        // Ignore the TCLLIBPATH environment variable when searching for packages.
        // We cannot do just Tcl_PutEnv("TCLLIBPATH=") here. In this case,
        // [info exists env(TCLLIBPATH)] will be true, but accessing $env(TCLLIBPATH)
        // element array results in a "no such variable" error.
        // Thus, here we will unset the array element.
        Tcl_UnsetVar2(interp, "env", "TCLLIBPATH", TCL_GLOBAL_ONLY);

        // Ignore variables in formats TCL<X>.<Y>_TM_PATH and TCL<X>_<Y>_TM_PATH,
        // where <X> is Tcl major version and <Y> is all previous minor versions.
        // These variables are used to specify the location of Tcl modules.
        // We don't allow you to use anything outside of VFS.
        for (int i = 0; i <= TCL_MINOR_VERSION; i++) {
            for (int j = 0; j < 2; j++) {
                Tcl_Obj *varName = Tcl_ObjPrintf("TCL" STRINGIFY(TCL_MAJOR_VERSION)
                    "%s%d_TM_PATH", (j ? "." : "_"), i);
                Tcl_IncrRefCount(varName);
                Tcl_UnsetVar2(interp, "env", Tcl_GetString(varName), TCL_GLOBAL_ONLY);
                Tcl_DecrRefCount(varName);
            }
        }

        // Now let's specify the path in VFS. Comments in source code say this:
        //
        // $tcl_library - can specify a primary location, if set, no other locations
        // will be checked. This is the recommended way for a program that embeds
        // Tcl to specifically tell Tcl where to find an init.tcl file.
        //
        // $env(TCL_LIBRARY) - highest priority so user can always override the search
        // path unless the application has specified an exact directory above.
        //
        // We cannot set the $tcl_library variable here because it will only be set
        // for this Tcl interpreter and not for child/threaded interpreters.
        // Thus, we choose to set environment variable TCL_LIBRARY.
        Tcl_PutEnv("TCL_LIBRARY=" VFS_MOUNT "lib/tcl" TCL_VERSION);
        Tcl_PutEnv("TK_LIBRARY=" VFS_MOUNT "lib/tk" TCL_VERSION);

    } else if (!g_isBootstrap) {
        DBG("Cookit_Startup: FATAL! vfs is not available");
        // If VFS unavailable and we are not in VFS bootstrap, throw an error.
        goto error;
    }

    DBG("Cookit_Startup: initialize interp...");
    if (Tcl_Init(interp) != TCL_OK) {
        goto error;
    }

    // Check if we have a wrapped script in VFS
    Tcl_Obj *wrappedScript = Tcl_NewStringObj(VFS_MOUNT "main.tcl", -1);
    // Tcl_FSAccess() must be called on object an with refcount >= 1.
    // Thus, we need to increment refcount for wrappedScript here.
    Tcl_IncrRefCount(wrappedScript);
    if (Tcl_FSAccess(wrappedScript, F_OK) == 0) {

        // We have wrapped script in VFS. Use it as startup script.
        DBG("Cookit_Startup: have wrapped script, use main.tcl");

        Tcl_SetStartupScript(wrappedScript, NULL);

        goto setNonInteractive;

    }

    DBG("Cookit_Startup: not wrapped");

    // Try to detect simple command like 'wrap' and 'stats' in command
    // line arguments.
    if (argvLength > 0) {

        // We do not check the error from Tcl_ListObjIndex() or check if
        // firstArg is NULL, since we made argvObj at the beginning of this
        // function and are sure that it is a list and contains at least
        // one element (because argvLength > 0).
        Tcl_Obj *firstArgObj;
        Tcl_ListObjIndex(NULL, argvObj, 0, &firstArgObj);
        Tcl_Size firstArgLen;
        const char *firstArgStr = Tcl_GetStringFromObj(firstArgObj, &firstArgLen);
        if (firstArgLen > 2 && firstArgStr[0] == '-' && firstArgStr[1] == '-') {
            // We found a builin command. Let's load the cookit package and run
            // builtin processing (rawStartup). Report error an if it failed.
            if (Tcl_EvalEx(interp, "package require cookit::builtin;"
                "::cookit::builtin::run", -1, TCL_EVAL_GLOBAL |
                TCL_EVAL_DIRECT) != TCL_OK)
            {
                goto error;
            }
            // ::cookit::builtin::run should call exit. Go to to done here.
            goto done;
        }

        DBG("Cookit_Startup: found startup script in cmd line args");

        // If the first argument is not a simple command, assume it is a script file.
        Tcl_SetStartupScript(firstArgObj, NULL);
        Tcl_SetVar2Ex(interp, "argv0", NULL, firstArgObj, TCL_GLOBAL_ONLY);
        // Remove it from $argv. The original argvObj also belongs to Tcl $argv
        // variable. We have to duplicate it.
        Tcl_Obj *argvObjDup = Tcl_DuplicateObj(argvObj);
        Tcl_ListObjReplace(NULL, argvObjDup, 0, 1, 0, NULL);
        Tcl_SetVar2Ex(interp, "argv", NULL, argvObjDup, TCL_GLOBAL_ONLY);
        // Decrease the number of arguments
        Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewIntObj(argvLength - 1),
            TCL_GLOBAL_ONLY);

        goto setNonInteractive;

    }

    // No startup script is specified on the command line.
    DBG("Cookit_Startup: no startup script in cmd line args");

    // Set the same startup file that tclsh/wish normally uses in case we are
    // running interactively
#ifndef COOKIT_CONSOLE_ONLY
    if (g_isConsoleMode) {
#endif /* COOKIT_CONSOLE_ONLY */
#ifdef __WIN32__
        Tcl_SetVar2Ex(interp, "tcl_rcFileName", NULL,
            Tcl_NewStringObj("~/tclshrc.tcl", -1), TCL_GLOBAL_ONLY);
#else
        Tcl_SetVar2Ex(interp, "tcl_rcFileName", NULL,
            Tcl_NewStringObj("~/.tclshrc", -1), TCL_GLOBAL_ONLY);
#endif /* __WIN32__ */
#ifndef COOKIT_CONSOLE_ONLY
    } else {
#ifdef __WIN32__
        Tcl_SetVar2Ex(interp, "tcl_rcFileName", NULL,
            Tcl_NewStringObj("~/wishrc.tcl", -1), TCL_GLOBAL_ONLY);
#else
        Tcl_SetVar2Ex(interp, "tcl_rcFileName", NULL,
            Tcl_NewStringObj("~/.wishrc", -1), TCL_GLOBAL_ONLY);
#endif /* __WIN32__ */
    }
#endif /* COOKIT_CONSOLE_ONLY */
    // In Tcl9+ we need to manually expand the tilde
#if TCL_MAJOR_VERSION > 8
    Tcl_EvalEx(interp, "set tcl_rcFileName [file tildeexpand $tcl_rcFileName]",
        -1, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
#endif /* TCL_MAJOR_VERSION > 8 */

    goto skipNonInteractive;

setNonInteractive:

    // If we are here, it means we found the main.tcl file in VFS or we need
    // to use a command line startup script. Otherwise, we have juped
    // to the skipNonInteractive: label.

    // Make sure we work in non-interactive mode
    Tcl_SetVar2Ex(interp, "tcl_interactive", NULL, Tcl_NewBooleanObj(0), TCL_GLOBAL_ONLY);

skipNonInteractive:

    // Release wrappedScript object that was created above.
    Tcl_DecrRefCount(wrappedScript);

    // Release argvObj object that was created above. We don't need it anymore.
    Tcl_DecrRefCount(argvObj);

    DBG("Cookit_Startup: before exit...");
#ifndef COOKIT_CONSOLE_ONLY
    if (!g_isConsoleMode) {
        DBG("Cookit_Startup: init GUI...");
#ifdef __WIN32__
        if (Tk_Init(interp) != TCL_OK)
            goto error;
        // Create console if we run without script and in GUI mode.
        // if (Tk_CreateConsoleWindow(interp) != TCL_OK)
        //    goto error;
#else
        if (Tcl_EvalEx(interp, "package require Tk", -1, TCL_EVAL_GLOBAL) != TCL_OK)
            goto error;
#endif /* __WIN32__ */
        // Create GUI console only if we are runing in interactive mode.
        // We expect that Tcl variable tcl_interactive is defined by Tcl_Main() / Tk_Main().
        if (strcmp(Tcl_GetVar(interp, "tcl_interactive", TCL_GLOBAL_ONLY), "1") == 0) {
            if (Tcl_EvalEx(interp, "package require cookit::console", -1, TCL_EVAL_GLOBAL) != TCL_OK)
                goto error;
        }
    }
#endif /* COOKIT_CONSOLE_ONLY */

done:

    DBG("Cookit_Startup: ok");
    return TCL_OK;

error:
    DBG("Cookit_Startup: RETURN ERROR");
#ifdef __WIN32__
    fprintf(stderr, "Fatal error: %s\n", Tcl_GetStringResult(interp));
    fflush(stderr);
#ifndef COOKIT_CONSOLE_ONLY
    if (!g_isConsoleMode) {
        MessageBeep(MB_ICONEXCLAMATION);
        MessageBoxA(NULL, Tcl_GetStringResult(interp), "Fatal error",
            MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    }
#endif /* COOKIT_CONSOLE_ONLY */
    ExitProcess(1);
#endif /* __WIN32__ */
    Tcl_DeleteInterp(interp);
    return TCL_ERROR;
}

#ifdef __WIN32__

#ifndef COOKIT_CONSOLE_ONLY
// This function is used to determine whether the current application
// is a GUI or a console. As for now, it check checks if if console is
// attached to the current process. This is not a 100% solution,
// as a console application may not have a console if it starts
// with CREATE_NO_WINDOW or is a Windows service. Since we may add
// more checks in the future, it has been placed in a separate function.
static inline int is_console(void) {
    if (GetConsoleWindow() == NULL) {
        return 0;
    } else {
        return 1;
    }
}
#endif /* COOKIT_CONSOLE_ONLY */

int _tmain(int argc, TCHAR *argv[]) {

    g_argc = argc;
    g_argv = argv;

#ifndef COOKIT_CONSOLE_ONLY
    if (is_console()) {
        g_isConsoleMode = GetEnvironmentVariableA("COOKIT_GUI",
            NULL, 0) == 0 ? 1 : 0;
    } else {
        g_isConsoleMode = GetEnvironmentVariableA("COOKIT_CONSOLE",
            NULL, 0) == 0 ? 0 : 1;
    }
#endif /* COOKIT_CONSOLE_ONLY */

    g_isBootstrap = GetEnvironmentVariableA("COOKIT_BOOTSTRAP",
        NULL, 0) == 0 ? 0 : 1;

#ifndef COOKIT_CONSOLE_ONLY
    if (g_isConsoleMode) {
#endif /* COOKIT_CONSOLE_ONLY */
        Tcl_Main(argc, argv, Cookit_Startup);
#ifndef COOKIT_CONSOLE_ONLY
    } else {
        Tk_Main(argc, argv, Cookit_Startup);
    }
#endif /* COOKIT_CONSOLE_ONLY */

    return TCL_OK;

}

#else

int main(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;

#ifndef COOKIT_CONSOLE_ONLY
    g_isConsoleMode = getenv("COOKIT_GUI") == NULL ? 1 : 0;
#endif /* COOKIT_CONSOLE_ONLY */
    g_isBootstrap = getenv("COOKIT_BOOTSTRAP") == NULL ? 0 : 1;

    Tcl_Main(argc, argv, Cookit_Startup);
    return TCL_OK;
}

#endif /* __WIN32__ */
