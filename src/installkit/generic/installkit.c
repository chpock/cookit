#ifdef __WIN32__
#define UNICODE
#define _UNICODE
#endif

#include <tcl.h>
#ifdef __WIN32__
#include <tk.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <Shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#endif /* __WIN32__ */

#include "crap.h"

#define streq(a,b) (a[0] == b[0] && strcmp(a,b) == 0)
#define tstreq(a,b) (a[0] == b[0] && wcscmp(a,b) == 0)

extern Tcl_AppInitProc Thread_Init;
extern Tcl_AppInitProc Miniarc_Init;
extern Tcl_AppInitProc Tk_Init;

static int
LibraryPathObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    if( objc == 1 ) {
        Tcl_SetObjResult( interp, (Tcl_Obj *)TclGetLibraryPath() );
    } else {
        Tcl_Obj *path = Tcl_DuplicateObj(objv[1]);
        TclSetLibraryPath( Tcl_NewListObj( 1, &path ) );
        TclpSetInitialEncodings();
    }

    return TCL_OK;
}

#ifdef STATIC_BUILD
static int
LoadTkObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    if( Tk_Init(interp) == TCL_ERROR ) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage( interp, "tk", Tk_Init, 0 );

    return TCL_OK;
}

#ifdef TCL_THREADS
static int
LoadThreadObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    if( Thread_Init( interp ) == TCL_ERROR ) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage( interp, "thread", Thread_Init, 0 );

    return TCL_OK;
}
#endif /* TCL_THREADS */
#endif /* STATIC_BUILD */

static int
LoadMiniarcObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    if( Miniarc_Init( interp ) == TCL_ERROR ) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage( interp, "miniarc", Miniarc_Init, 0 );

    return TCL_OK;
}

#ifdef __WIN32__
static int
WindowsCreateShortcutObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    HRESULT hres;
    int error = 1;
    int i, len, index, iconIdx, showCommand;
    char *opt, *val;
    TCHAR *linkPath;

    Tcl_DString dsArgs, dsDesc, dsIcon;
    Tcl_DString dsPath, dsTarget, dsWork;

    IShellLinkW  *psl = NULL;
    IPersistFile *ppf = NULL;
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

    const char *switches[] = {
        "-arguments", "-description", "-icon", "-objectpath",
        "-showcommand", "-workingdirectory", (char *)NULL
    };

    enum {
        OPT_ARGUMENTS, OPT_DESCRIPTION, OPT_ICON, OPT_OBJECTPATH,
        OPT_SHOWCOMMAND, OPT_WORKINGDIRECTORY
    };

    const char *showcommands[] = {
        "hidden", "maximized", "minimized", "normal", (char *)NULL
    };

    enum {
        SHOW_HIDDEN, SHOW_MAXIMIZED, SHOW_MINIMIZED, SHOW_NORMAL
    };

    if( objc < 2 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "shortcutPath ?options?" );
        return TCL_ERROR;
    }

    CoInitialize(NULL);

    hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            &IID_IShellLinkW, (LPVOID*)&psl);

    if( FAILED(hres) ) {
        Tcl_SetStringObj( resultObj, "failed to create COM instance", -1 );
        goto error;
    }

    for(i = 2; i < objc; ++i) {
        opt = Tcl_GetString(objv[i]);

        if( Tcl_GetIndexFromObj( interp, objv[i], switches,
            "option", 0, &index ) != TCL_OK ) { goto error; }

        /*
         * If there's no argument after the switch, give them some help
         * and return an error.
         */
        if( ++i == objc ) {
            Tcl_AppendStringsToObj( resultObj, opt,
                " option must be followed by an argument", (char *)NULL );
            goto error;
        }

        val = Tcl_GetString(objv[i]);

        switch(index) {
        case OPT_ARGUMENTS:
            hres = psl->lpVtbl->SetArguments(psl,
                Tcl_WinUtfToTChar(val, -1, &dsArgs));
            break;
        case OPT_DESCRIPTION:
            hres = psl->lpVtbl->SetDescription(psl,
                Tcl_WinUtfToTChar(val, -1, &dsDesc));
            break;
        case OPT_ICON:
            if( ++i == objc ) {
                Tcl_SetStringObj( resultObj,
                    "-icon option must have iconPath and iconIndex", -1 );
                goto error;
            }

            if(Tcl_GetIntFromObj(interp, objv[i], &iconIdx) == TCL_ERROR) {
                goto error;
            }

            hres = psl->lpVtbl->SetIconLocation(psl,
                Tcl_WinUtfToTChar(val, -1, &dsIcon),iconIdx);
            break;
        case OPT_OBJECTPATH:
            hres = psl->lpVtbl->SetPath(psl,
                Tcl_WinUtfToTChar(val, -1, &dsTarget));
            break;
        case OPT_SHOWCOMMAND:
            if( Tcl_GetIndexFromObj( interp, objv[i], showcommands,
                "showcommand", 0, &index ) != TCL_OK ) { goto error; }

            switch(index) {
            case SHOW_HIDDEN:
                showCommand = SW_HIDE;
                break;
            case SHOW_MAXIMIZED:
                showCommand = SW_SHOWMAXIMIZED;
                break;
            case SHOW_MINIMIZED:
                showCommand = SW_SHOWMINNOACTIVE;
                break;
            case SHOW_NORMAL:
                showCommand = SW_SHOWNORMAL;
                break;
            }

            hres = psl->lpVtbl->SetShowCmd(psl, showCommand);
            break;
        case OPT_WORKINGDIRECTORY:
            hres = psl->lpVtbl->SetWorkingDirectory(psl,
                Tcl_WinUtfToTChar(val, -1, &dsWork));
            break;
        }

        if (FAILED(hres)) {
            Tcl_AppendStringsToObj( resultObj, "failed to set ", opt,
                " option to ", val, (char *)NULL );
            goto error;
        }
    }

    hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (LPVOID*)&ppf);

    if (FAILED(hres)) {
        Tcl_SetStringObj( resultObj, "failed to query file interface", -1 );
        goto error;
    }

    /* Save the link  */
    linkPath = Tcl_WinUtfToTChar(Tcl_GetString(objv[1]), -1, &dsPath);
    hres = ppf->lpVtbl->Save(ppf, linkPath, TRUE);
    ppf->lpVtbl->Release(ppf);

    error = 0;

error:
    if (psl) {
        psl->lpVtbl->Release(psl);
    }

    Tcl_DStringFree(&dsArgs);
    Tcl_DStringFree(&dsDesc);
    Tcl_DStringFree(&dsIcon);
    Tcl_DStringFree(&dsPath);
    Tcl_DStringFree(&dsTarget);
    Tcl_DStringFree(&dsWork);

    CoUninitialize();

    return error ? TCL_ERROR : TCL_OK;
}

struct csidl {
    char name[32];
    int  csidl;
};

int
WindowsGetFolderObjCmd(
    ClientData clientData,
    Tcl_Interp*interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int idx;
    TCHAR path[MAX_PATH];
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "folder");
        return TCL_ERROR;
    }

    const char *folders[] = {
        "ADMINTOOLS",
        "APPDATA",
        "ALTSTARTUP",
        "BITBUCKET",
        "CDBURN_AREA",
        "COMMON_ADMINTOOLS",
        "COMMON_APPDATA",
        "COMMON_ALTSTARTUP",
        "COMMON_DESKTOPDIRECTORY",
        "COMMON_DOCUMENTS",
        "COMMON_FAVORITES",
        "COMMON_MUSIC",
        "COMMON_OEM_LINKS",
        "COMMON_PICTURES",
        "COMMON_PROGRAMS",
        "COMMON_STARTMENU",
        "COMMON_STARTUP",
        "COMMON_TEMPLATES",
        "COMMON_VIDEO",
        "COMPUTERSNEARME",
        "CONNECTIONS",
        "CONTROLS",
        "COOKIES",
        "DESKTOP",
        "DESKTOPDIRECTORY",
        "DRIVES",
        "FAVORITES",
        "FONTS",
        "HISTORY",
        "INTERNET",
        "INTERNET_CACHE",
        "LOCAL_APPDATA",
        "MYDOCUMENTS",
#ifdef CSIDL_MYMUSIC
        "MYMUSIC",
#endif
        "MYPICTURES",
#ifdef CSIDL_MYVIDEO
        "MYVIDEO",
#endif
        "NETHOOD",
        "NETWORK",
        "PERSONAL",
        "PROGRAM_FILES",
        "PROGRAM_FILES_COMMON",
        "PROGRAM_FILES_COMMONX86",
        "PROGRAM_FILESX86",
        "PROGRAMS",
        "PRINTERS",
        "PRINTHOOD",
        "PROFILE",
        "RECENT",
        "RESOURCES",
        "RESOURCES_LOCALIZED",
        "SENDTO",
        "STARTMENU",
        "STARTUP",
        "SYSTEM",
        "SYSTEMX86",
        "TEMPLATES",
        "WINDOWS",
        (char)NULL
    };

    const int csidls[] = {
        CSIDL_ADMINTOOLS,
        CSIDL_APPDATA,
        CSIDL_ALTSTARTUP,
        CSIDL_BITBUCKET,
        CSIDL_CDBURN_AREA,
        CSIDL_COMMON_ADMINTOOLS,
        CSIDL_COMMON_APPDATA,
        CSIDL_COMMON_ALTSTARTUP,
        CSIDL_COMMON_DESKTOPDIRECTORY,
        CSIDL_COMMON_DOCUMENTS,
        CSIDL_COMMON_FAVORITES,
        CSIDL_COMMON_MUSIC,
        CSIDL_COMMON_OEM_LINKS,
        CSIDL_COMMON_PICTURES,
        CSIDL_COMMON_PROGRAMS,
        CSIDL_COMMON_STARTMENU,
        CSIDL_COMMON_STARTUP,
        CSIDL_COMMON_TEMPLATES,
        CSIDL_COMMON_VIDEO,
        CSIDL_COMPUTERSNEARME,
        CSIDL_CONNECTIONS,
        CSIDL_CONTROLS,
        CSIDL_COOKIES,
        CSIDL_DESKTOP,
        CSIDL_DESKTOPDIRECTORY,
        CSIDL_DRIVES,
        CSIDL_FAVORITES,
        CSIDL_FONTS,
        CSIDL_HISTORY,
        CSIDL_INTERNET,
        CSIDL_INTERNET_CACHE,
        CSIDL_LOCAL_APPDATA,
        CSIDL_PERSONAL,
#ifdef CSIDL_MYMUSIC
        CSIDL_MYMUSIC,
#endif
        CSIDL_MYPICTURES,
#ifdef CSIDL_MYVIDEO
        CSIDL_MYVIDEO,
#endif
        CSIDL_NETHOOD,
        CSIDL_NETWORK,
        CSIDL_PERSONAL,
        CSIDL_PROGRAM_FILES,
        CSIDL_PROGRAM_FILES_COMMON,
        CSIDL_PROGRAM_FILES_COMMONX86,
        CSIDL_PROGRAM_FILESX86,
        CSIDL_PROGRAMS,
        CSIDL_PRINTERS,
        CSIDL_PRINTHOOD,
        CSIDL_PROFILE,
        CSIDL_RECENT,
        CSIDL_RESOURCES,
        CSIDL_RESOURCES_LOCALIZED,
        CSIDL_SENDTO,
        CSIDL_STARTMENU,
        CSIDL_STARTUP,
        CSIDL_SYSTEM,
        CSIDL_SYSTEMX86,
        CSIDL_TEMPLATES,
        CSIDL_WINDOWS
    };

    if (Tcl_GetIndexFromObj(interp, objv[1], folders, "option", 0, &idx)
        != TCL_OK) return TCL_ERROR;

    if (SUCCEEDED(SHGetFolderPath(NULL, csidls[idx], NULL, 0, path))) {
        Tcl_DString ds;
        Tcl_SetStringObj(resultObj,
            Tcl_WinTCharToUtf((const TCHAR *)path, -1, &ds), -1);
        Tcl_DStringFree(&ds);
    }

    return TCL_OK;
}

int
WindowsDriveObjCmd(
    ClientData clientData,
    Tcl_Interp*interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int idx;
    Tcl_Obj *resultObj = Tcl_GetObjResult( interp );
    const char *subCommands[] = {
        "list", "type", "size", "freespace", (char *)NULL
    };

    enum {
        CMD_LIST, CMD_TYPE, CMD_SIZE, CMD_FREESPACE
    };

    if( objc < 2 ) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ..?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], subCommands, "option", 0, &idx)
        != TCL_OK) return TCL_ERROR;

    switch( idx ) {
    case CMD_LIST:
    {
        Tcl_Obj *listObj = Tcl_NewListObj( 0, NULL );
        DWORD len;
        DWORD dslen;
        LPTSTR driveStrings;
        TCHAR *pDriveString;

        /* Find out how big this string will be. */
        len = GetLogicalDriveStrings( 0, NULL ) + 2;

        /* Allocate the memory. */
        driveStrings = (TCHAR *)Tcl_Alloc( len );

        GetLogicalDriveStrings( len, driveStrings );

        pDriveString = driveStrings;
        dslen = wcslen( pDriveString );

        while( dslen > 0 ) {
            Tcl_DString ds;
            char *driveName = Tcl_WinTCharToUtf(pDriveString, -1, &ds);
            Tcl_Obj *drive = Tcl_NewStringObj( driveName, -1 );
            Tcl_ListObjAppendElement( interp, listObj, drive );
            Tcl_DStringFree(&ds);

            pDriveString += dslen + 1;
            dslen = wcslen( pDriveString );
        }

        Tcl_Free( (char *)driveStrings );
        Tcl_SetObjResult( interp, listObj );
        break;
    }

    case CMD_TYPE:
    {
        int size;
        unsigned int type;
        char *drive;
        Tcl_Obj *string;
        Tcl_DString ds;

        if( objc < 3 ) {
            Tcl_WrongNumArgs( interp, 1, objv, "type drive" );
            return TCL_ERROR;
        }

        drive  = Tcl_GetStringFromObj( objv[2], &size );
        string = Tcl_NewStringObj( drive, -1 );

        if( drive[size - 1] != '\\' ) {
            Tcl_AppendToObj( string, "\\", 1 );
        }
        type = GetDriveType(Tcl_WinUtfToTChar(Tcl_GetString(string), -1, &ds) );
        Tcl_DStringFree(&ds);

        switch( type ) {
        case DRIVE_UNKNOWN:
            string = Tcl_NewStringObj( "unknown", -1 );
            break;
        case DRIVE_NO_ROOT_DIR:
            string = Tcl_NewStringObj( "no root dir", -1 );
            break;
        case DRIVE_REMOVABLE:
            string = Tcl_NewStringObj( "removable", -1 );
            break;
        case DRIVE_FIXED:
            string = Tcl_NewStringObj( "fixed", -1 );
            break;
        case DRIVE_REMOTE:
            string = Tcl_NewStringObj( "remote", -1 );
            break;
        case DRIVE_CDROM:
            string = Tcl_NewStringObj( "cdrom", -1 );
            break;
        case DRIVE_RAMDISK:
            string = Tcl_NewStringObj( "ramdisk", -1 );
            break;
        }

        Tcl_SetObjResult( interp, string );
        break;
    }

    case CMD_SIZE:
    {
        int size;
        char *drive;
        Tcl_Obj *string;
        Tcl_DString ds;
        ULONGLONG value = 0;
        ULARGE_INTEGER freeBytes;
        ULARGE_INTEGER totalBytes;
        ULARGE_INTEGER totalFreeBytes;

        if( objc < 3 ) {
            Tcl_WrongNumArgs( interp, 1, objv, "size drive" );
            return TCL_ERROR;
        }

        drive  = Tcl_GetStringFromObj( objv[2], &size );
        string = Tcl_NewStringObj( drive, -1 );

        if( drive[size - 1] != '\\' ) {
            Tcl_AppendToObj( string, "\\", 1 );
        }

        if( GetDiskFreeSpaceEx(
            Tcl_WinUtfToTChar(Tcl_GetString(string), -1, &ds),
            &freeBytes,
            &totalBytes,
            &totalFreeBytes) ) {

            value = totalBytes.QuadPart;
        }
        Tcl_DStringFree(&ds);

        Tcl_SetObjResult( interp, Tcl_NewWideIntObj(value) );
        break;
    }

    case CMD_FREESPACE:
    {
        int size;
        int userFree = 0;
        char *drive;
        Tcl_Obj *string;
        Tcl_DString ds;
        ULONGLONG value = 0;
        ULARGE_INTEGER freeBytes;
        ULARGE_INTEGER totalBytes;
        ULARGE_INTEGER totalFreeBytes;

        if( objc < 3 || objc > 4 ) {
            Tcl_WrongNumArgs( interp, 1, objv, "freespace ?-user? drive" );
            return TCL_ERROR;
        }

        drive  = Tcl_GetStringFromObj( objv[2], &size );

        if( objc == 4 ) {
            if( !Tcl_StringMatch( Tcl_GetString(objv[2]), "-user" ) ) {
                Tcl_WrongNumArgs( interp, 1, objv, "freespace ?-user? drive" );
                return TCL_ERROR;
            }
            userFree = 1;
            drive = Tcl_GetStringFromObj( objv[3], &size );
        }

        string = Tcl_NewStringObj( drive, -1 );

        if( drive[size - 1] != '\\' ) {
            Tcl_AppendToObj( string, "\\", 1 );
        }

        if( GetDiskFreeSpaceEx(
            Tcl_WinUtfToTChar(Tcl_GetString(string), -1, &ds),
            &freeBytes,
            &totalBytes,
            &totalFreeBytes) ) {

            if( userFree ) {
                value = freeBytes.QuadPart;
            } else {
                value = totalFreeBytes.QuadPart;
            }
        }
        Tcl_DStringFree(&ds);

        Tcl_SetObjResult( interp, Tcl_NewWideIntObj(value) );
        break;
    }

    }

    return TCL_OK;
}

int
WindowsGuidObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    GUID guid;

    if( FAILED( CoCreateGuid(&guid) ) ) {
        Tcl_SetStringObj( Tcl_GetObjResult(interp),
                "could not generate GUID", -1 );
        return TCL_ERROR;
    } else {
        char *str;
        TCHAR *tstr;
        Tcl_DString ds;
        Tcl_Obj *string = Tcl_NewObj();

        UuidToString( (UUID *)&guid, &tstr );
        str = Tcl_WinTCharToUtf(tstr, -1, &ds);
        Tcl_AppendStringsToObj( string, "{", str, "}", (char *)NULL );
        Tcl_SetObjResult( interp, string );
        Tcl_DStringFree(&ds);
        RpcStringFree(&tstr);
    }

    return TCL_OK;
}

int
WindowsTrashObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int i, idx;
    Tcl_Obj *path;
    SHFILEOPSTRUCT s;
    Tcl_DString ds;
    TCHAR *p, buf[MAX_PATH+1];

    const char *options[] = {
        "-noconfirmation", "-noerrorui", "-silent", "-simpleprogress",
        (char *)NULL
    };

    enum {
        OPT_NOCONFIRMATION, OPT_NOERRORUI, OPT_SILENT, OPT_SIMPLEPROGRESS
    };

    if( objc < 2 ) {
        Tcl_WrongNumArgs(interp, 1, objv, "?options? file ?file ...?");
        return TCL_ERROR;
    }

    memset(&s, 0, sizeof(SHFILEOPSTRUCT));
    s.wFunc = FO_DELETE;
    s.fFlags = FOF_ALLOWUNDO;
    for (i = 1; i < objc; ++i) {
        char *opt = Tcl_GetString(objv[i]);

        if (opt[0] != '-') break;

        if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0, &idx)
            != TCL_OK) return TCL_ERROR;

        switch (idx) {
            case OPT_NOCONFIRMATION:
                s.fFlags |= FOF_NOCONFIRMATION;
                break;
            case OPT_NOERRORUI:
                s.fFlags |= FOF_NOERRORUI;
                break;
            case OPT_SILENT:
                s.fFlags |= FOF_SILENT;
                break;
            case OPT_SIMPLEPROGRESS:
                s.fFlags |= FOF_SIMPLEPROGRESS;
                break;
        }
    }

    if (i >= objc) {
        Tcl_WrongNumArgs(interp, 1, objv, "?options? file ?file ...?");
        return TCL_ERROR;
    }

    for (; i < objc; ++i) {
        memset(buf, 0, sizeof(buf));
        path = Tcl_FSGetNormalizedPath(interp, objv[i]);
        if (!path) continue;
        s.pFrom = p = Tcl_WinUtfToTChar(Tcl_GetString(path), -1, &ds);
        while (*p++) if (*p == '/') *p = '\\';
        SHFileOperation(&s);
        Tcl_DStringFree(&ds);
    }

    return TCL_OK;
}

int
WindowsShellExecuteObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int i, idx;
    int result = TCL_OK;
    int wait = 0, state = SW_NORMAL;
    Tcl_Obj *pathObj, *workDirObj = NULL;
    Tcl_DString dsCmd, dsPath, dsWork, dsArgs;
    TCHAR *cmd, *path, *args = NULL, *workingdir = NULL;
    TCHAR *def = L"default";

    const char *options[] = {
        "-wait", "-windowstate", "-workingdirectory", (char *)NULL
    };

    enum {
        OPT_WAIT, OPT_WINDOWSTATE, OPT_WORKINGDIRECTORY
    };

    const char *subcmds[] = {
        "default", "edit", "explore", "find", "open", "print", (char *)NULL
    };

    enum {
        CMD_DEFAULT, CMD_EDIT, CMD_EXPLORE, CMD_FIND, CMD_OPEN, CMD_PRINT
    };

    const char *windowStates[] = {
        "hidden", "maximized", "minimized", "normal", (char *)NULL
    };

    enum {
        STATE_HIDDEN, STATE_MAXIMIZED, STATE_MINIMIZED, STATE_NORMAL
    };

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "?options? command file ?arguments?");
        return TCL_ERROR;
    }

    for (i = 1; i < objc; ++i) {
        char *opt = Tcl_GetString(objv[i]);

        if (opt[0] != '-') break;

        if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
                &idx) != TCL_OK) return TCL_ERROR;

        switch (idx) {
            case OPT_WAIT:
                wait = 1;
                break;
            case OPT_WINDOWSTATE:
                if (Tcl_GetIndexFromObj(interp, objv[++i], windowStates,
                        "windowstate", 0, &idx) != TCL_OK) return TCL_ERROR;
                switch (idx) {
                    case STATE_HIDDEN:
                        state = SW_HIDE;
                        break;
                    case STATE_MAXIMIZED:
                        state = SW_MAXIMIZE;
                        break;
                    case STATE_MINIMIZED:
                        state = SW_MINIMIZE;
                        break;
                    case STATE_NORMAL:
                        state = SW_NORMAL;
                        break;
                }
                break;
            case OPT_WORKINGDIRECTORY:
                workDirObj = objv[++i];
                break;
        }
    }

    if (((objc - i) < 2) || ((objc - i) > 3)) {
        Tcl_WrongNumArgs(interp, 1, objv, "?options? command file ?arguments?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[i], subcmds, "command", TCL_EXACT,
            &idx) != TCL_OK) return TCL_ERROR;
    cmd = Tcl_WinUtfToTChar(Tcl_GetString(objv[i++]), -1, &dsCmd);
    if (tstreq(cmd, def)) cmd = NULL;

    path = Tcl_WinUtfToTChar(Tcl_GetString(objv[i++]), -1, &dsPath);

    if (workDirObj) {
        workingdir = Tcl_WinUtfToTChar(Tcl_GetString(workDirObj), -1, &dsWork);
    }

    if ((objc - i) == 1) {
        args = Tcl_WinUtfToTChar(Tcl_GetString(objv[i]), -1, &dsArgs);
    }

    CoInitialize(NULL);
    if (wait) {
        SHELLEXECUTEINFO si = {0};
        si.cbSize = sizeof(SHELLEXECUTEINFO);
        si.fMask = SEE_MASK_NOCLOSEPROCESS;
        si.hwnd = NULL;
        si.lpVerb = cmd;
        si.lpFile = path;
        si.lpParameters = args;
        si.lpDirectory = workingdir;
        si.nShow = state;
        si.hInstApp = NULL;

        if (ShellExecuteEx(&si)) {
            if (si.hProcess) {
                WaitForSingleObject(si.hProcess, INFINITE);
            }
        } else {
            result = TCL_ERROR;
        }
    } else {
        if ((int)ShellExecute(NULL, cmd, path, args, workingdir, state) <= 32) {
            result = TCL_ERROR;
        }
    }
    CoUninitialize();

error:
    if (result == TCL_ERROR) {
        Tcl_AppendToObj(Tcl_GetObjResult(interp), "unknown error occurred", -1);
    }
    Tcl_DStringFree(&dsPath);
    if (workDirObj) Tcl_DStringFree(&dsWork);
    return result;
}

int wow64FsRedirected = 0;
PVOID wow64RedirOldVal;
typedef BOOL (WINAPI *WowFsFunc)(PVOID *);

int
WindowsDisableWow64FsRedirection(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    WowFsFunc func;
    BOOL res = FALSE;
    TCHAR *lib = L"kernel32.dll";
    HMODULE hLib = LoadLibrary(lib);

    if (hLib) {
        func = (WowFsFunc)GetProcAddress(hLib,"Wow64DisableWow64FsRedirection");
        if (func) res = func(&wow64RedirOldVal);
        if (res) wow64FsRedirected = 1;
    }

    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), res);
    FreeLibrary(hLib);
    return TCL_OK;
}

int
WindowsRevertWow64FsRedirection(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    WowFsFunc func;
    BOOL res = FALSE;
    TCHAR *lib = L"kernel32.dll";
    HMODULE hLib = LoadLibrary(lib);

    if (wow64FsRedirected && hLib) {
        func = (WowFsFunc)GetProcAddress(hLib, "Wow64RevertWow64FsRedirection");
        if (func) res = func(wow64RedirOldVal);
        if (res) wow64FsRedirected = 0;
    }

    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), res);
    FreeLibrary(hLib);
    return TCL_OK;
}
#endif /* __WIN32__ */

extern void TclX_IdInit( Tcl_Interp *interp );
extern void TclX_OsCmdsInit( Tcl_Interp *interp );

int
Installkit_Init( Tcl_Interp *interp )
{
    if( Crapvfs_Init(interp) == TCL_ERROR ) {
        return TCL_ERROR;
    }

    Tcl_StaticPackage( interp, "installkit", Installkit_Init, 0 );

    Tcl_CreateObjCommand( interp, "installkit::librarypath", LibraryPathObjCmd,
        0, 0 );

#ifdef STATIC_BUILD
    Tcl_CreateObjCommand( interp, "installkit::loadTk",
        LoadTkObjCmd, 0, 0 );
#ifdef TCL_THREADS
    Tcl_CreateObjCommand( interp, "installkit::loadThread",
        LoadThreadObjCmd, 0, 0 );
#endif /* TCL_THREADS */
#endif /* STATIC_BUILD */

    Tcl_CreateObjCommand( interp, "installkit::loadMiniarc",
        LoadMiniarcObjCmd, 0, 0 );

#ifdef __WIN32__
    Tcl_CreateObjCommand( interp, "installkit::Windows::guid",
            WindowsGuidObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp, "installkit::Windows::drive",
            WindowsDriveObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp, "installkit::Windows::trash",
            WindowsTrashObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp, "installkit::Windows::getFolder",
            WindowsGetFolderObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp, "installkit::Windows::shellExecute",
            WindowsShellExecuteObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp, "installkit::Windows::createShortcut",
            WindowsCreateShortcutObjCmd, 0, 0);
    Tcl_CreateObjCommand( interp,
            "installkit::Windows::disableWow64FsRedirection",
            WindowsDisableWow64FsRedirection, 0, 0);
    Tcl_CreateObjCommand( interp,
            "installkit::Windows::revertWow64FsRedirection",
            WindowsRevertWow64FsRedirection, 0, 0);

#else
    /* Add the "id" command from TclX on UNIX. */
    TclX_IdInit(interp);
#endif /* __WIN32__ */

    /* Add the "system" command from TclX. */
    TclX_InitCommands(interp);

    Zlib_Init(interp);
    Zvfs_Init(interp);

    return TCL_OK;
}
