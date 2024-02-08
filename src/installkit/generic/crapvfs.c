#include <zlib.h>
#include "tclInt.h"
#include "tclPort.h"
#include "crap.h"
#include "crapvfs.h"
#include "LZMADecode.h"

/*
 * Size of the decompression input buffer
 */
#define COMPR_BUF_SIZE 16384

#ifdef __WIN32__
#define NOCASE_PATHS 1
#else
#define NOCASE_PATHS 0
#endif

#define STREQU( a, b ) (a[0] == b[0] && strcmp(a,b) == 0)


/*
 * All static variables are collected into a structure named "local".
 * That way, it is clear in the code when we are using a static
 * variable because its name begins with "local.".
 */
static struct {
    Tcl_HashTable fileHash;     /* One entry for each file in the crapvfs.
                                 * The key is the virtual filename.  The data
                                 * an an instance of the CrapvfsFile structure.
                                 */
    Tcl_HashTable archiveHash;  /* One entry for each archive.
                                 * Key is the name.
                                 * Data is the CrapvfsArchive structure.
                                 */
    Tcl_Obj *passwordObj;

    int isInit;                 /* True after initialization */
} local;

/*
 * Each CRAP archive file that is mounted is recorded as an instance
 * of this structure
 */
typedef struct CrapvfsArchive {
    int refCount;
    int fullyMounted;
    Tcl_WideInt lastOffset;
    char version[2];
    Tcl_Obj *zName;         /* Name of the archive */
    Tcl_Obj *zMountPoint;   /* Where this archive is mounted */
} CrapvfsArchive;

/*
 * Particulars about each virtual file are recorded in an instance
 * of the following structure.
 */
typedef struct CrapvfsFile {
    int refCount;                /* Reference count */
    Tcl_Obj *zName;              /* The full pathname of the virtual file */
    CrapvfsArchive *pArchive;    /* The CRAP archive holding this file data */
    int isdir;                   /* Whether the file is a directory record */
    int method;                  /* The method of compression used */
    int timestamp;               /* Stored timestamp of the file */
    Tcl_WideInt offset;          /* Offset into the CRAP archive of the data */
    Tcl_WideInt nByte;           /* Uncompressed size of the virtual file */
    Tcl_WideInt nByteCompr;      /* Compressed size of the virtual file */
    int encrypted;               /* Whether this file is stored encrypted */
    struct CrapvfsFile *parent;  /* Parent directory. */
    Tcl_HashTable children;      /* For directory entries, a hash table of
                                  * all of the files in the directory.
                                  */
} CrapvfsFile;

/*
 * Whenever a crapvfs file is opened, an instance of this structure is
 * attached to the open channel where it will be available to the
 * crapvfs I/O routines below.  All state information about an open
 * crapvfs file is held in this structure.
 */
typedef struct CrapvfsChannelInfo {
    Tcl_WideInt nData;        /* total number of bytes of compressed data */
    Tcl_WideInt nByte;        /* number of bytes of read uncompressed data */
    Tcl_WideInt nByteCompr;   /* number of bytes of unread compressed data */
    Tcl_WideInt readSoFar;    /* Number of bytes read so far */
    Tcl_WideInt startOfData;  /* File position of data in CRAP archive */
    int method;               /* Compression method */
    int encrypted;            /* Whether the file is encrypted */
    Tcl_Channel chan;         /* Open to the archive file */
    unsigned char *zBuf;      /* buffer used by the decompressor */
    z_stream zlibStream;      /* state of the zlib decompressor */
    lzma_stream lzmaStream;   /* state of the lzma decompressor */
    RC4_CTX ctx;
} CrapvfsChannelInfo;

/* The attributes defined for each file in the archive.
 * These are accessed via the 'file attributes' command in Tcl.
 */
static CONST char *CrapvfsAttrs[] = {
    "-archive", "-compressedsize", "-method", "-mount",
    "-offset", "-uncompressedsize", (char *)NULL
};

enum {
    CRAPVFS_ATTR_ARCHIVE, CRAPVFS_ATTR_COMPSIZE, CRAPVFS_ATTR_METHOD,
    CRAPVFS_ATTR_MOUNT, CRAPVFS_ATTR_OFFSET, CRAPVFS_ATTR_UNCOMPSIZE
};

/* Forward declarations for the callbacks to the Tcl filesystem. */

static Tcl_FSPathInFilesystemProc       PathInFilesystem;
static Tcl_FSDupInternalRepProc		DupInternalRep;
static Tcl_FSFreeInternalRepProc	FreeInternalRep;
static Tcl_FSInternalToNormalizedProc	InternalToNormalized;
static Tcl_FSFilesystemPathTypeProc     FilesystemPathType;
static Tcl_FSFilesystemSeparatorProc    FilesystemSeparator;
static Tcl_FSStatProc                   Stat;
static Tcl_FSAccessProc                 Access;
static Tcl_FSOpenFileChannelProc        OpenFileChannel;
static Tcl_FSMatchInDirectoryProc       MatchInDirectory;
static Tcl_FSListVolumesProc            ListVolumes;
static Tcl_FSFileAttrStringsProc        FileAttrStrings;
static Tcl_FSFileAttrsGetProc           FileAttrsGet;
static Tcl_FSFileAttrsSetProc           FileAttrsSet;
static Tcl_FSChdirProc                  Chdir;

static Tcl_Filesystem crapvfsFilesystem = {
    "crapvfs",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    &PathInFilesystem,
    &DupInternalRep,
    &FreeInternalRep,
    &InternalToNormalized,
    NULL,			/* &CreateInternalRep, */
    NULL,                       /* &NormalizePath, */
    &FilesystemPathType,
    &FilesystemSeparator,
    &Stat,
    &Access,
    &OpenFileChannel,
    &MatchInDirectory,
    NULL,                       /* &Utime, */
    NULL,                       /* &Link, */
    &ListVolumes,
    &FileAttrStrings,
    &FileAttrsGet,
    &FileAttrsSet,
    NULL,			/* &CreateDirectory, */
    NULL,			/* &RemoveDirectory, */
    NULL,			/* &DeleteFile, */
    NULL,			/* &CopyFile, */
    NULL,			/* &RenameFile, */
    NULL,			/* &CopyDirectory, */
    NULL,                       /* &Lstat, */
    NULL,			/* &LoadFile, */
    NULL,			/* &GetCwd, */
    &Chdir
};


/*
 * Forward declarations describing the channel type structure for
 * opening and reading files inside of an archive.
 */
static Tcl_DriverCloseProc        DriverClose;
static Tcl_DriverInputProc        DriverInput;
static Tcl_DriverOutputProc       DriverOutput;
static Tcl_DriverSeekProc         DriverSeek;
static Tcl_DriverWatchProc        DriverWatch;
static Tcl_DriverGetHandleProc    DriverGetHandle;

static Tcl_ChannelType vfsChannelType = {
    "crapvfs",                /* Type name.                                 */
    TCL_CHANNEL_VERSION_2, /* Set blocking/nonblocking behaviour. NULL'able */
    DriverClose,           /* Close channel, clean instance data            */
    DriverInput,           /* Handle read request                           */
    DriverOutput,          /* Handle write request                          */
    DriverSeek,            /* Move location of access point.      NULL'able */
    NULL,                  /* Set options.                        NULL'able */
    NULL,                  /* Get options.                        NULL'able */
    DriverWatch,           /* Initialize notifier                           */
    DriverGetHandle        /* Get OS handle from the channel.               */
};


/*
 *----------------------------------------------------------------------
 *
 * StrDup --
 *
 * 	Create a copy of the given string and lower it if necessary.
 *
 * Results:
 * 	Pointer to the new string.  Space to hold the returned
 * 	string is obtained from Tcl_Alloc() and should be freed
 * 	by the calling function.
 *
 *----------------------------------------------------------------------
 */

static char *
StrDup( char *str, int lower )
{
    int i, c, len;
    char *newstr;

    len = strlen(str);

    newstr = Tcl_Alloc( len + 1 );
    memcpy( newstr, str, len );
    newstr[len] = '\0';

    if( lower ) {
        for( i = 0; (c = newstr[i]) != 0; ++i )
        {
            if( isupper(c) ) {
                newstr[i] = tolower(c);
            }
        }
    }

    return newstr;
}

/*
 *----------------------------------------------------------------------
 *
 * CanonicalPath --
 *
 * 	Concatenate zTail onto zRoot to form a pathname.  After
 * 	concatenation, simplify the pathname by removing ".." and
 * 	"." directories.
 *
 * Results:
 * 	Pointer to the new pathname.  Space to hold the returned
 * 	path is obtained from Tcl_Alloc() and should be freed by
 * 	the calling function.
 *
 *----------------------------------------------------------------------
 */

static char *
CanonicalPath( const char *zRoot, const char *zTail )
{
    char *zPath;
    int i, j, c;
    int len = strlen(zRoot) + strlen(zTail) + 2;

#ifdef __WIN32__
    if( isalpha(zTail[0]) && zTail[1]==':' ){ zTail += 2; }
    if( zTail[0]=='\\' ){ zRoot = ""; zTail++; }
    if( zTail[0]=='\\' ){ zRoot = "/"; zTail++; } /*account for UNC style path*/
#endif
    if( zTail[0]=='/' ){ zRoot = ""; zTail++; }
    if( zTail[0]=='/' ){ zRoot = "/"; zTail++; } /*account for UNC style path*/

    zPath = Tcl_Alloc( len );
    if( !zPath ) return NULL;

    sprintf( zPath, "%s/%s", zRoot, zTail );
    for( i = j = 0; (c = zPath[i]) != 0; i++ )
    {
#ifdef __WIN32__
        if( c == '\\' ) {
            c = '/';
        }
#endif
        if( c == '/' ) {
            int c2 = zPath[i+1];
            if( c2 == '/' ) continue;
            if( c2 == '.' ) {
                int c3 = zPath[i+2];
                if( c3 == '/' || c3 == 0 ) {
                    i++;
                    continue;
                }
                if( c3 == '.' && (zPath[i+3] == '.' || zPath[i+3] == 0) ) {
                    i += 2;
                    while( j > 0 && zPath[j-1] != '/' ) { j--; }
                    continue;
                }
            }
        }
        zPath[j++] = c;
    }

    if( j == 0 ) {
        zPath[j++] = '/';
    }

    zPath[j] = 0;

    return zPath;
}

/*
 *----------------------------------------------------------------------
 *
 * AbsolutePath --
 *
 * 	Construct an absolute pathname from the given pathname.  On
 * 	Windows, all backslash (\) characters are converted to
 * 	forward slash (/), and if NOCASE_PATHS is true, all letters
 * 	are converted to lowercase.  The drive letter, if present, is
 * 	preserved.
 *
 * Results:
 * 	Pointer to the new pathname.  Space to hold the returned
 * 	path is obtained from Tcl_Alloc() and should be freed by
 * 	the calling function.
 *
 *----------------------------------------------------------------------
 */

static char *
AbsolutePath( const char *z )
{
    int len;
    char *zResult;

    if( *z != '/'
#ifdef __WIN32__
        && *z != '\\' && (!isalpha(*z) || z[1] != ':' )
#endif
    ) {
        /* Case 1:  "z" is a relative path, so prepend the current
         * working directory in order to generate an absolute path.
         */
        Tcl_Obj *pwd = Tcl_FSGetCwd(NULL);
        zResult = CanonicalPath( Tcl_GetString(pwd), z );
        Tcl_DecrRefCount(pwd);
    } else {
        /* Case 2:  "z" is an absolute path already, so we
         * just need to make a copy of it.
         */
        zResult = StrDup( (char *)z, 0);
    }

    /* If we're on Windows, we want to convert all backslashes to
     * forward slashes.  If NOCASE_PATHS is true, we want to also
     * lower the alpha characters in the path.
     */
#if NOCASE_PATHS || defined(__WIN32__)
    {
        int i, c;
        for( i = 0; (c = zResult[i]) != 0; i++ )
        {
#if NOCASE_PATHS
            if( isupper(c) ) {
                zResult[i] = tolower(c);
            }
#endif
#ifdef __WIN32__
            if( c == '\\' ) {
                zResult[i] = '/';
            }
#endif
        }
    }
#endif /* NOCASE_PATHS || defined(__WIN32__) */

    len = strlen(zResult);
    /* Strip the trailing / from any directory. */
    if( zResult[len-1] == '/' ) {
        zResult[len-1] = 0;
    }

    return zResult;
}

/*
 *----------------------------------------------------------------------
 *
 * AddPathToArchive --
 *
 * 	Add the given pathname to the given archive.  zName is usually
 * 	the pathname pulled from the file header in a crap archive.  We
 * 	concatenate it onto the archive's mount point to obtain a full
 * 	path before adding it to our hash table.
 *
 * 	All parent directories of the given path will be created and
 * 	added to the hash table.
 *
 * Results:
 * 	Pointer to the new file structure or to the old file structure
 * 	if it already existed.  newPath will be true if this path is
 * 	new to this archive or false if we already had it.
 *
 *----------------------------------------------------------------------
 */

static CrapvfsFile *
AddPathToArchive( CrapvfsArchive *pArchive, char *zName, int *newPath )
{
    int i, len, isNew;
    char *zFullPath, *izFullPath;
    char *zParentPath, *izParentPath;
    Tcl_HashEntry *pEntry;
    Tcl_Obj *nameObj, *pathObj, *listObj;
    CrapvfsFile *pCrapvfs, *parent = NULL;

    zFullPath = CanonicalPath( Tcl_GetString(pArchive->zMountPoint), zName );
    izFullPath = zFullPath;

    pathObj = Tcl_NewStringObj( zFullPath, -1 );
    Tcl_IncrRefCount( pathObj );

    listObj = Tcl_FSSplitPath( pathObj, &len );
    Tcl_IncrRefCount( listObj );
    Tcl_DecrRefCount( pathObj );

    /* Walk through all the parent directories of this
     * file and add them all to our archive.  This is
     * because some crap files don't store directory
     * entries in the archive, but we need to know all
     * of the directories to create the proper filesystem.
     */
    for( i = 1; i < len; ++i )
    {
        pathObj = Tcl_FSJoinPath( listObj, i );

        izParentPath = zParentPath  = Tcl_GetString(pathObj);
#if NOCASE_PATHS
        izParentPath = StrDup( zParentPath, 1 );
#endif
        pEntry = Tcl_CreateHashEntry( &local.fileHash, izParentPath, &isNew );
#if NOCASE_PATHS
        Tcl_Free( izParentPath );
#endif

        if( !isNew ) {
            /* We already have this directory in our archive. */
            parent = Tcl_GetHashValue( pEntry );
            continue;
        }

        Tcl_ListObjIndex( NULL, listObj, i-1, &nameObj );
        Tcl_IncrRefCount(nameObj);

        /* We don't have this directory in our archive yet.  Add it. */
        pCrapvfs = (CrapvfsFile*)Tcl_Alloc( sizeof(*pCrapvfs) );
        pCrapvfs->refCount   = 1;
        pCrapvfs->zName      = nameObj;
        pCrapvfs->pArchive   = pArchive;
        pCrapvfs->isdir      = 1;
        pCrapvfs->offset     = 0;
        pCrapvfs->nByte      = 0;
        pCrapvfs->nByteCompr = 0;
        pCrapvfs->parent     = parent;
        Tcl_InitHashTable( &pCrapvfs->children, TCL_STRING_KEYS );

        Tcl_SetHashValue( pEntry, pCrapvfs );

        if( parent ) {
            /* Add this directory to its parent's list of children. */
            pEntry = Tcl_CreateHashEntry(&parent->children,zParentPath,&isNew);
            if( isNew ) {
                Tcl_SetHashValue( pEntry, pCrapvfs );
            }
        }

        parent = pCrapvfs;
    }

    /* Check to see if we already have this file in our archive. */
#if NOCASE_PATHS
    izFullPath = StrDup( zFullPath, 1 );
#endif
    pEntry = Tcl_CreateHashEntry(&local.fileHash, izFullPath, newPath);
#if NOCASE_PATHS
    Tcl_Free( izFullPath );
#endif

    if( *newPath ) {
        /* We don't have this file in our archive.  Add it. */
        Tcl_ListObjIndex( NULL, listObj, len-1, &nameObj );
        Tcl_IncrRefCount(nameObj);

        pCrapvfs = (CrapvfsFile*)Tcl_Alloc( sizeof(*pCrapvfs) );
        pCrapvfs->refCount   = 1;
        pCrapvfs->zName      = nameObj;
        pCrapvfs->pArchive   = pArchive;

        Tcl_SetHashValue( pEntry, pCrapvfs );

        /* Add this path to its parent's list of children. */
        pEntry = Tcl_CreateHashEntry(&parent->children, zFullPath, &isNew);

        if( isNew ) {
            Tcl_SetHashValue( pEntry, pCrapvfs );
        }
    } else {
        /* We already have this file.  Set the pointer and return. */
        pCrapvfs = Tcl_GetHashValue( pEntry );
    }

    Tcl_DecrRefCount(listObj);
    Tcl_Free(zFullPath);

    return pCrapvfs;
}

/*
 *----------------------------------------------------------------------
 *
 * Crapvfs_Mount --
 *
 * 	Read a crap archive and make entries in the file hash table for
 * 	all of the files in the archive.  If Crapvfs has not been initialized,
 * 	it will be initialized here before mounting the archive.
 *
 * Results:
 * 	Standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

int
Crapvfs_Mount(
    Tcl_Interp *interp,        /* Leave error messages in this interpreter */
    CONST char *zArchive,      /* The CRAP archive file */
    CONST char *zMountPoint    /* Mount contents at this directory */
) {
    Tcl_Channel chan = NULL;   /* Used for reading the CRAP archive file */
    char *zArchiveName = 0;    /* A copy of zArchive */
    char *zFullMountPoint = 0; /* Absolute path to the mount point */
    int res;
    int code = TCL_ERROR;      /* Return code */
    int update = 1;            /* Whether to update the mounts */
    int crapVersion = 2;
    CrapvfsArchive *pArchive;  /* The CRAP archive being mounted */
    Tcl_HashEntry *pEntry;     /* Hash table entry */
    int isNew;                 /* Flag to tell use when a hash entry is new */
    CrapvfsFile *pCrapvfs;     /* A new virtual file */
    CrapCentralHeader central;
    CrapFileHeader header;
    Tcl_Obj *hashKeyObj = NULL;
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
    Tcl_Obj *offsetNameObj, *headersNameObj;

    if( !local.isInit ) {
        if( Crapvfs_Init( interp ) == TCL_ERROR ) {
            Tcl_SetStringObj( resultObj, "failed to initialize crapvfs", -1 );
            return TCL_ERROR;
        }
    }

    /* If zArchive is NULL, set the result to a list of all
     * mounted files.
     */
    if( !zArchive ) {
        Tcl_HashSearch zSearch;

        for( pEntry = Tcl_FirstHashEntry( &local.archiveHash,&zSearch );
                pEntry; pEntry = Tcl_NextHashEntry(&zSearch) )
        {
            if( (pArchive = Tcl_GetHashValue(pEntry)) ) {
                Tcl_ListObjAppendElement( interp, resultObj,
                        Tcl_DuplicateObj(pArchive->zName) );
            }
        }
        code = TCL_OK;
        update = 0;
        goto done;
    }

    /* If zMountPoint is NULL, set the result to the mount point
     * for the specified archive file.
     */
    if( !zMountPoint ) {
        int found = 0;
        Tcl_HashSearch zSearch;

        zArchiveName = AbsolutePath( zArchive );
        for( pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
                pEntry; pEntry = Tcl_NextHashEntry(&zSearch) )
        {
            pArchive = Tcl_GetHashValue(pEntry);
            if ( !strcmp( Tcl_GetString(pArchive->zName), zArchiveName ) ) {
                ++found;
                Tcl_SetStringObj( resultObj,
                        Tcl_GetString(pArchive->zMountPoint), -1 );
                break;
            }
        }

        if( !found ) {
            Tcl_SetStringObj( resultObj, "archive not mounted by crapvfs", -1 );
        }

        code = found ? TCL_OK : TCL_ERROR;
        update = 0;
        goto done;
    }

    if(!(chan = CrapOpen(interp, zArchive, "r"))) {
        goto done;
    }

    zArchiveName    = AbsolutePath( zArchive );
    zFullMountPoint = AbsolutePath( zMountPoint );

    hashKeyObj = Tcl_NewObj();
    Tcl_IncrRefCount(hashKeyObj);
    Tcl_AppendStringsToObj( hashKeyObj, zArchiveName, ":", zFullMountPoint,
            (char *)NULL );

    pEntry = Tcl_CreateHashEntry( &local.archiveHash,
            Tcl_GetString(hashKeyObj), &isNew );

    if( !isNew ) {
        /* This archive is already mounted.  Set the result to
         * the current mount point and return.
         */
        pArchive = Tcl_GetHashValue(pEntry);
        if (pArchive->fullyMounted) {
            code = TCL_OK;
            update = 0;
            goto done;
        }

        Tcl_Seek(chan, pArchive->lastOffset, SEEK_SET);
    } else {
        CrapReadCentralHeader(interp, chan, &central);

        pArchive = (CrapvfsArchive*)Tcl_Alloc(sizeof(*pArchive));
        pArchive->refCount     = 1;
        pArchive->fullyMounted = 0;
        pArchive->lastOffset   = 0;
        pArchive->zName        = Tcl_NewStringObj(zArchiveName,-1);
        pArchive->zMountPoint  = Tcl_NewStringObj(zFullMountPoint,-1);
        strncpy(pArchive->version, central.version, 2);
        Tcl_SetHashValue(pEntry, pArchive);

        /* Add the root mount point to our list of files as a directory. */
        pEntry = Tcl_CreateHashEntry(&local.fileHash, zFullMountPoint, &isNew);

        if( isNew ) {
            pCrapvfs = (CrapvfsFile*)Tcl_Alloc( sizeof(*pCrapvfs) );
            pCrapvfs->refCount   = 1;
            pCrapvfs->zName      = Tcl_NewStringObj(zFullMountPoint,-1);
            pCrapvfs->pArchive   = pArchive;
            pCrapvfs->isdir      = 1;
            pCrapvfs->offset     = 0;
            pCrapvfs->nByte      = 0;
            pCrapvfs->nByteCompr = 0;
            pCrapvfs->parent     = NULL;
            Tcl_InitHashTable( &pCrapvfs->children, TCL_STRING_KEYS );

            Tcl_SetHashValue( pEntry, pCrapvfs );
        }
    }

    while((res = CrapNextFile(interp, chan, &header)) == TCL_OK) {
        pCrapvfs = AddPathToArchive( pArchive, header.name, &isNew );

        pCrapvfs->isdir      = 0;
        pCrapvfs->method     = CRAP_METHOD_NONE;
        pCrapvfs->offset     = Tcl_Tell(chan);
        pCrapvfs->encrypted  = 0;
        pCrapvfs->timestamp  = strtowide(header.mtime);
        pCrapvfs->nByte      = strtowide(header.size);
        pCrapvfs->nByteCompr = strtowide(header.csize);

        if( header.encrypted[0] == 'E' ) {
            pCrapvfs->encrypted = 1;
        }

        if( header.method[0] == 'Z' ) {
            pCrapvfs->method = CRAP_METHOD_ZLIB;
        } else if( header.method[0] == 'L' ) {
            pCrapvfs->method = CRAP_METHOD_LZMA;
        }

        if( pArchive->version[1] == '2' ) {
            pCrapvfs->offset = strtowide(header.offset);
        } else {
            Tcl_Seek( chan, pCrapvfs->nByteCompr, SEEK_CUR );
        }

        if (!pArchive->fullyMounted && STREQU(header.name, "splash.tcl")) {
            pArchive->lastOffset = Tcl_Tell(chan);
            code = CRAPVFS_MOUNT_PARTIAL;
            goto done;
        }
    }

    if (res == TCL_ERROR) {
        code = TCL_ERROR;
    }

    code = TCL_OK;
    pArchive->fullyMounted = 1;

done:
    if( chan ) Tcl_Close( interp, chan );

    if( hashKeyObj ) Tcl_DecrRefCount(hashKeyObj);

    if( zArchiveName ) Tcl_Free(zArchiveName);
    if( zFullMountPoint ) Tcl_Free(zFullMountPoint);

    if( code == TCL_OK ) {
        Tcl_SetStringObj( resultObj, zMountPoint, -1 );

        if( update ) {
            Tcl_FSMountsChanged( &crapvfsFilesystem );
        }
    }

    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Crapvfs_Unmount --
 *
 * 	Unmount all the files in the given crap archive.  All the
 * 	entries in the file hash table for the archive are deleted
 * 	as well as the entry in the archive hash table.
 *
 * 	Any memory associated with the entries will be freed as well.
 *
 * Results:
 * 	Standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

int
Crapvfs_Unmount( Tcl_Interp *interp, CONST char *zMountPoint )
{
    int found = 0;
    CrapvfsFile *pFile;
    CrapvfsArchive *pArchive;
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch zSearch;
    Tcl_HashEntry *fEntry;
    Tcl_HashSearch fSearch;

    for( pEntry = Tcl_FirstHashEntry( &local.archiveHash, &zSearch );
            pEntry; pEntry = Tcl_NextHashEntry(&zSearch) )
    {
        pArchive = Tcl_GetHashValue(pEntry);
        if( !Tcl_StringCaseMatch( zMountPoint,
                Tcl_GetString(pArchive->zMountPoint), NOCASE_PATHS ) ) continue;

        found++;

        for( fEntry = Tcl_FirstHashEntry( &local.fileHash, &fSearch );
                fEntry; fEntry = Tcl_NextHashEntry(&fSearch) )
        {
            pFile = Tcl_GetHashValue(fEntry);
            if( pFile->pArchive == pArchive ) {
                FreeInternalRep( (ClientData)pFile );
                Tcl_DeleteHashEntry(fEntry);
            }
        }

        Tcl_DeleteHashEntry(pEntry);
        Tcl_DecrRefCount(pArchive->zName);
        Tcl_DecrRefCount(pArchive->zMountPoint);
        Tcl_Free( (char *)pArchive );
    }

    if( !found ) {
        if( interp ) {
            Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                    zMountPoint, " is not a crapvfs mount", (char *)NULL );
        }
        return TCL_ERROR;
    }

    Tcl_FSMountsChanged( &crapvfsFilesystem );

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CrapvfsLookup --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 *
 * 	Look into the file hash table for a given path and see if
 * 	it belongs to our filesystem.
 *
 * Results:
 * 	Pointer to the file structure or NULL if it was not found.
 *
 *----------------------------------------------------------------------
 */

static CrapvfsFile *
CrapvfsLookup( Tcl_Obj *pathPtr )
{
    char *zTrueName;
    Tcl_HashEntry *pEntry;

    zTrueName = AbsolutePath( Tcl_GetString(pathPtr) );
    pEntry = Tcl_FindHashEntry( &local.fileHash, zTrueName );
    Tcl_Free(zTrueName);

    return pEntry ? Tcl_GetHashValue(pEntry) : NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetCrapvfsFile --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 *
 * 	For a given pathPtr, return the internal representation
 * 	of the path for our filesystem.
 *
 * Results:
 * 	Pointer to the file structure or NULL if it was not found.
 *
 *----------------------------------------------------------------------
 */

static CrapvfsFile *
GetCrapvfsFile( Tcl_Obj *pathPtr )
{
    CrapvfsFile *pFile =
        (CrapvfsFile *)Tcl_FSGetInternalRep(pathPtr,&crapvfsFilesystem);
    return pFile == NULL || pFile->pArchive->refCount == 0 ? NULL : pFile;
}

/*
 *----------------------------------------------------------------------
 *
 * CrapvfsFileMatchesType --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 *
 * 	See if the given CrapvfsFile matches the type data given.
 *
 * Results:
 * 	1 if true, 0 if false
 *
 *----------------------------------------------------------------------
 */

static int
CrapvfsFileMatchesType( CrapvfsFile *pFile, Tcl_GlobTypeData *types )
{
    if( types ) {
        if( types->type & TCL_GLOB_TYPE_FILE && pFile->isdir ) {
            return 0;
        }

        if( types->type & (TCL_GLOB_TYPE_DIR | TCL_GLOB_TYPE_MOUNT)
                && !pFile->isdir ) {
            return 0;
        }

        if( types->type & TCL_GLOB_TYPE_MOUNT && pFile->parent ) {
            return 0;
        }
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverExit --
 *
 * 	This function is called as an exit handler for the channel
 * 	driver.  If we do not set pInfo.chan to NULL, Tcl_Close()
 * 	will be called twice on that channel when Tcl_Exit runs.
 * 	This will lead to a core dump
 *
 * Results:
 * 	None
 *
 *----------------------------------------------------------------------
 */

static void
DriverExit( void *pArg )
{
    CrapvfsChannelInfo *pInfo = (CrapvfsChannelInfo*)pArg;
    pInfo->chan = 0;
}


/*
 *----------------------------------------------------------------------
 *
 * DriverClose --
 *
 * 	Called when a channel is closed.
 *
 * Results:
 * 	Returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */

static int
DriverClose(
  ClientData  instanceData,    /* A CrapvfsChannelInfo structure */
  Tcl_Interp *interp           /* The TCL interpreter */
) {
    CrapvfsChannelInfo* pInfo = (CrapvfsChannelInfo*)instanceData;

    if( pInfo->zBuf ) {
        Tcl_Free(pInfo->zBuf);
    }

    if ( pInfo->method == CRAP_METHOD_ZLIB ) {
        inflateEnd(&pInfo->zlibStream);
    } else if ( pInfo->method == CRAP_METHOD_LZMA ) {
        lzmaEnd(&pInfo->lzmaStream);
    }

    if( pInfo->chan ) {
        Tcl_Close(interp, pInfo->chan);
        Tcl_DeleteExitHandler(DriverExit, pInfo);
    }

    Tcl_Free((char*)pInfo);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverInput --
 *
 * 	The Tcl channel system calls this function on each read
 * 	from a channel.  The channel is opened into the actual
 * 	archive file, but the data is read from the individual
 * 	file entry inside the crap archive.
 *
 * Results:
 * 	Number of bytes read.
 *
 *----------------------------------------------------------------------
 */

static int
DriverInput (
  ClientData instanceData, /* The channel to read from */
  char *buf,               /* Buffer to fill */
  int toRead,              /* Requested number of bytes */
  int *pErrorCode          /* Location of error flag */
) {
    int decrypt = 0;
    CrapvfsChannelInfo* pInfo = (CrapvfsChannelInfo*) instanceData;

    if( pInfo->encrypted && local.passwordObj != NULL ) {
        decrypt = 1;
    }

    if( toRead > pInfo->nByte ) {
        toRead = pInfo->nByte;
    }

    if( toRead == 0 ) {
        return 0;
    }

    if( pInfo->method == CRAP_METHOD_NONE ) {
        /* No compression.  Just read the channel. */
        toRead = Tcl_Read(pInfo->chan, buf, toRead);
        if( decrypt ) {
            rc4_crypt( &pInfo->ctx, buf, toRead );
        }
    } else if( pInfo->method == CRAP_METHOD_ZLIB ) {
        /* Zlib compression */

        int err = Z_OK;
        z_stream *stream = &pInfo->zlibStream;
        stream->next_out  = buf;
        stream->avail_out = toRead;
        while (stream->avail_out) {
            if (!stream->avail_in) {
                Tcl_WideInt len = pInfo->nByteCompr;
                if (len > COMPR_BUF_SIZE) {
                    len = COMPR_BUF_SIZE;
                }
                len = Tcl_Read(pInfo->chan, pInfo->zBuf, len);
                if( decrypt ) {
                    rc4_crypt( &pInfo->ctx, pInfo->zBuf, len );
                }

                pInfo->nByteCompr -= len;
                stream->next_in  = pInfo->zBuf;
                stream->avail_in = len;
            }

            err = inflate(stream, Z_NO_FLUSH);
            if (err) break;
        }

        if (err == Z_STREAM_END) {
            if ((stream->avail_out != 0)) {
                *pErrorCode = err; /* premature end */
                return -1;
            }
        } else if( err ) {
            *pErrorCode = err; /* some other zlib error */
            return -1;
        }
    } else if ( pInfo->method == CRAP_METHOD_LZMA ) {
        /* LZMA compression */

        int err = LZMA_OK;
        lzma_stream *stream = &pInfo->lzmaStream;
        stream->next_out  = buf;
        stream->avail_out = toRead;
        while (stream->avail_out) {
            if (!stream->avail_in) {
                Tcl_WideInt len = pInfo->nByteCompr;
                if (len > COMPR_BUF_SIZE) {
                    len = COMPR_BUF_SIZE;
                }
                len = Tcl_Read(pInfo->chan, pInfo->zBuf, len);
                if( decrypt ) {
                    rc4_crypt( &pInfo->ctx, pInfo->zBuf, len );
                }

                pInfo->nByteCompr -= len;
                stream->next_in  = pInfo->zBuf;
                stream->avail_in = len;
            }

            if( (err = lzmaDecode(stream)) ) break;
        }

        if( err == LZMA_DATA_ERROR ) {
            *pErrorCode = EBADF; /* some lzma data error */
            return -1;
        }
    }

    pInfo->nByte     -= toRead;
    pInfo->readSoFar += toRead;
    *pErrorCode = 0;

    return toRead;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverOutput --
 *
 * 	Called to write to a file.  Since this is a read-only file
 * 	system, this function will always return an error.
 *
 * Results:
 * 	Returns -1.
 *
 *----------------------------------------------------------------------
 */

static int
DriverOutput(
  ClientData instanceData,   /* The channel to write to */
  CONST char *buf,                 /* Data to be stored. */
  int toWrite,               /* Number of bytes to write. */
  int *pErrorCode            /* Location of error flag. */
) {
    *pErrorCode = EINVAL;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverSeek --
 *
 * 	Seek along the open channel to another point.
 *
 * Results:
 * 	Offset into the file.
 *
 *----------------------------------------------------------------------
 */

static int
DriverSeek(
  ClientData instanceData,    /* The file structure */
  long offset,                /* Offset to seek to */
  int mode,                   /* One of SEEK_CUR, SEEK_SET or SEEK_END */
  int *pErrorCode             /* Write the error code here */
){
    CrapvfsChannelInfo* pInfo = (CrapvfsChannelInfo*) instanceData;

    switch( mode ) {
    case SEEK_CUR:
        offset += pInfo->readSoFar;
        break;
    case SEEK_END:
        offset += pInfo->readSoFar + pInfo->nByte;
        break;
    default:
        /* Do nothing */
        break;
    }

    if( pInfo->method == CRAP_METHOD_NONE ) {
        /* dont seek behind end of data */
	if (pInfo->nData < (unsigned long)offset) {
	    return -1;
        }

	/* do the job, save and check the result */
	offset = Tcl_Seek(pInfo->chan, offset + pInfo->startOfData, SEEK_SET);
	if (offset == -1) {
	    return -1;
        }

	 /* adjust the counters (use real offset) */
	pInfo->readSoFar = offset - pInfo->startOfData;
	pInfo->nByte = pInfo->nData - pInfo->readSoFar; 
    } else if ( pInfo->method == CRAP_METHOD_ZLIB ) {
        if( offset < pInfo->readSoFar ) {
            z_stream *stream = &pInfo->zlibStream;
            inflateEnd(stream);
            stream->zalloc   = (alloc_func)0;
            stream->zfree    = (free_func)0;
            stream->opaque   = (voidpf)0;
            stream->avail_in = 2;
            stream->next_in  = pInfo->zBuf;
            pInfo->zBuf[0]   = 0x78;
            pInfo->zBuf[1]   = 0x01;
            inflateInit(stream);

            Tcl_Seek( pInfo->chan, pInfo->startOfData, SEEK_SET );
            pInfo->nByte += pInfo->readSoFar;
            pInfo->nByteCompr = pInfo->nData;
            pInfo->readSoFar = 0;
        }

        while( pInfo->readSoFar < offset )
        {
            int toRead, errCode;
            char zDiscard[100];
            toRead = offset - pInfo->readSoFar;
            if( toRead>sizeof(zDiscard) ) toRead = sizeof(zDiscard);
            DriverInput(instanceData, zDiscard, toRead, &errCode);
        }
    } else if ( pInfo->method == CRAP_METHOD_LZMA ) {
        if( offset < pInfo->readSoFar ) {
            lzma_stream *stream = &pInfo->lzmaStream;
            lzmaInit(stream);
            Tcl_Seek(pInfo->chan, pInfo->startOfData, SEEK_SET);
            pInfo->nByte += pInfo->readSoFar;
            pInfo->nByteCompr = pInfo->nData;
            pInfo->readSoFar = 0;
        }

        while( pInfo->readSoFar < offset )
        {
            int toRead, errCode;
            char zDiscard[100];
            toRead = offset - pInfo->readSoFar;
            if( toRead > sizeof(zDiscard) ) toRead = sizeof(zDiscard);
            DriverInput(instanceData, zDiscard, toRead, &errCode);
        }
    }

    *pErrorCode = 0;

    return pInfo->readSoFar;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverWatch --
 *
 * 	Called to handle events on the channel.  Since crapvfs files
 * 	don't generate events, this is a no-op.
 *
 * Results:
 * 	None
 *
 *----------------------------------------------------------------------
 */

static void
DriverWatch(
  ClientData instanceData,   /* Channel to watch */
  int mask                   /* Events of interest */
) {
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * DriverGetHandle --
 *
 * 	Retrieve a device-specific handle from the given channel.
 * 	Since we don't have a device-specific handle, this is a no-op.
 *
 * Results:
 * 	Returns TCL_ERROR.
 *
 *----------------------------------------------------------------------
 */

static int
DriverGetHandle(
  ClientData  instanceData,   /* Channel to query */
  int direction,              /* Direction of interest */
  ClientData* handlePtr       /* Space to the handle into */
) {
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PathInFilesystem --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Check to see if the given path is part of our filesystem.
 * 	We check the file hash table for the path, and if we find
 * 	it, set clientDataPtr to the CrapvfsFile pointer so that Tcl
 * 	will cache it for later.
 *
 * Results:
 * 	TCL_OK on success, or -1 on failure
 *
 *----------------------------------------------------------------------
 */

static int
PathInFilesystem( Tcl_Obj *pathPtr, ClientData *clientDataPtr )
{
    CrapvfsFile *pFile = CrapvfsLookup(pathPtr);

    if( pFile ) {
        *clientDataPtr = DupInternalRep((ClientData)pFile);
        return TCL_OK;
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * DupInternalRep --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Duplicate the CrapvfsFile "native" rep of a path.
 *
 * Results:
 * 	Returns clientData, with refcount incremented.
 *
 *----------------------------------------------------------------------
 */

static ClientData
DupInternalRep( ClientData clientData )
{
    CrapvfsFile *pFile = (CrapvfsFile *)clientData;
    pFile->refCount++;
    return (ClientData)pFile;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeInternalRep --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Free one reference to the CrapvfsFile "native" rep of a path.
 * 	When all references are gone, free the struct.
 *
 * Side effects:
 * 	May free memory.
 *
 *----------------------------------------------------------------------
 */

static void
FreeInternalRep( ClientData clientData )
{
    CrapvfsFile *pFile = (CrapvfsFile *)clientData;

    if (--pFile->refCount <= 0) {
        if( pFile->isdir ) {
            /* Delete the hash table containing the children
             * of this directory.  We don't need to free the
             * data for each entry in the table because they're
             * just pointers to the CrapvfsFiles, and those will
             * be freed below.
             */
            Tcl_DeleteHashTable( &pFile->children );
        }
        Tcl_DecrRefCount(pFile->zName);
        Tcl_Free((char *)pFile);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InternalToNormalized --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	From a CrapvfsFile representation, produce the path string rep.
 *
 * Results:
 * 	Returns a Tcl_Obj holding the string rep.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
InternalToNormalized( ClientData clientData )
{
    CrapvfsFile *pFile = (CrapvfsFile *)clientData;
    if( !pFile->parent ) {
        return Tcl_DuplicateObj( pFile->zName );
    } else {
        return Tcl_FSJoinToPath( pFile->parent->zName, 1, &pFile->zName );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FilesystemPathType --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Used for informational purposes only.  Return a Tcl_Obj
 * 	which describes the "type" of path this is.  For our
 * 	little filesystem, they're all "CRAP".
 *
 * Results:
 * 	Tcl_Obj with 0 refCount
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
FilesystemPathType( Tcl_Obj *pathPtr )
{
    return Tcl_NewStringObj( "CRAP", -1 );
}

/*
 *----------------------------------------------------------------------
 *
 * FileSystemSeparator --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Return a Tcl_Obj describing the separator character for
 * 	our filesystem.  We like things the old-fashioned way,
 * 	so we'll just use /.
 *
 * Results:
 * 	Tcl_Obj with 0 refCount
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
FilesystemSeparator( Tcl_Obj *pathPtr )
{ 
    return Tcl_NewStringObj( "/", -1 );
}

/*
 *----------------------------------------------------------------------
 *
 * Stat --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Does a stat() system call for a crapvfs file.  Fill the stat
 * 	buf with as much information as we have.
 *
 * Results:
 * 	0 on success, -1 on failure.
 *
 *----------------------------------------------------------------------
 */

static int
Stat( Tcl_Obj *pathPtr, Tcl_StatBuf *buf )
{
    CrapvfsFile *pFile;

    if( !(pFile = GetCrapvfsFile(pathPtr)) ) {
        return -1;
    }

    memset(buf, 0, sizeof(*buf));
    if (pFile->isdir) {
        buf->st_mode = 040555;
    } else {
        buf->st_mode = 0100555;
    }

    buf->st_size  = pFile->nByte;
    buf->st_mtime = pFile->timestamp;
    buf->st_ctime = pFile->timestamp;
    buf->st_atime = pFile->timestamp;

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Access --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Does an access() system call for a crapvfs file.
 *
 * Results:
 * 	0 on success, -1 on failure.
 *
 *----------------------------------------------------------------------
 */

static int
Access( Tcl_Obj *pathPtr, int mode )
{
    if( mode & 3 || !GetCrapvfsFile(pathPtr) ) return -1;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenFileChannel --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 *
 * 	Called when Tcl wants to open a file inside a crapvfs file system.
 * 	We actually open the crap file back up and seek to the offset
 * 	of the given file.  The channel driver will take care of the
 * 	rest.
 *
 * Results:
 * 	New channel on success, NULL on failure.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Channel
OpenFileChannel( Tcl_Interp *interp, Tcl_Obj *pathPtr,
    int mode, int permissions )
{
    CrapvfsFile *pFile;
    CrapvfsChannelInfo *pInfo;
    Tcl_Channel chan;
    static int count = 1;
    char zName[50];

    if( !(pFile = GetCrapvfsFile(pathPtr)) ) {
        return NULL;
    }

    if(!(chan = Tcl_OpenFileChannel(interp,
                    Tcl_GetString(pFile->pArchive->zName), "r", 0))) {
        return NULL;
    }

    if( Tcl_SetChannelOption(interp, chan, "-translation", "binary") ) {
        /* this should never happen */
        Tcl_Close( NULL, chan );
        return NULL;
    }

    pInfo = (CrapvfsChannelInfo*)Tcl_Alloc( sizeof(*pInfo) );
    pInfo->chan       = chan;
    pInfo->method     = pFile->method;
    pInfo->encrypted  = pFile->encrypted;
    pInfo->nByte      = pFile->nByte;
    pInfo->nData      = pFile->nByteCompr;
    pInfo->nByteCompr = pFile->nByteCompr;
    pInfo->readSoFar  = 0;

    if( pInfo->encrypted && local.passwordObj != NULL ) {
        rc4_init( local.passwordObj, &pInfo->ctx );
    }

    Tcl_CreateExitHandler( DriverExit, pInfo );

    if( pInfo->method == CRAP_METHOD_ZLIB ) {
        z_stream *stream = &pInfo->zlibStream;
        pInfo->zBuf      = Tcl_Alloc(COMPR_BUF_SIZE);
        stream->zalloc   = (alloc_func)0;
        stream->zfree    = (free_func)0;
        stream->opaque   = (voidpf)0;
        stream->avail_in = 2;
        stream->next_in  = pInfo->zBuf;
        pInfo->zBuf[0]   = 0x78;
        pInfo->zBuf[1]   = 0x01;
        inflateInit(stream);
    } else if ( pInfo->method == CRAP_METHOD_LZMA ) {
	lzma_stream *stream = &pInfo->lzmaStream;
        pInfo->zBuf      = Tcl_Alloc(COMPR_BUF_SIZE);
        stream->avail_in = 0;
        stream->next_in  = pInfo->zBuf;
        lzmaInit(stream);
    } else {
        pInfo->zBuf      = 0;
    }

    Tcl_Seek( chan, pFile->offset, SEEK_SET );
    pInfo->startOfData = Tcl_Tell(chan);

    sprintf( zName, "crapvfs%x%x", ((int)pFile)>>12, count++ );

    return Tcl_CreateChannel( &vfsChannelType, zName, 
                                (ClientData)pInfo, TCL_READABLE );
}

/*
 *----------------------------------------------------------------------
 *
 * MatchInDirectory --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Called when Tcl is globbing around through the filesystem.
 * 	This function can be called when Tcl is looking for mount
 * 	points or when it is looking for files within a mount point
 * 	that it has already determined belongs to us.
 *
 * 	Any matching file in our filesystem is appended to the
 * 	result pointer.
 *
 * Results:
 * 	Standard Tcl result
 *
 *----------------------------------------------------------------------
 */

/* Function to process a 'MatchInDirectory()'.
 * If not implemented, then glob and recursive
 * copy functionality will be lacking in the filesystem.
 */
static int
MatchInDirectory(
    Tcl_Interp* interp,
    Tcl_Obj *result,
    Tcl_Obj *pathPtr,
    CONST char *pattern,
    Tcl_GlobTypeData *types
) {
    CrapvfsFile *pFile;
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch sSearch;

    if( types && types->type & TCL_GLOB_TYPE_MOUNT ) {
        /* Tcl is looking for a list of our mount points that
         * match the given pattern.  This is so that Tcl can
         * append vfs mounted directories to a list of actual
         * filesystem directories.
         */
        char *path, *zPattern;
        CrapvfsArchive *pArchive;
        Tcl_Obj *patternObj = Tcl_NewObj();

        path = AbsolutePath( Tcl_GetString(pathPtr) );
        Tcl_AppendStringsToObj( patternObj, path, "/", pattern, (char *)NULL );
        Tcl_Free(path);
        zPattern = Tcl_GetString( patternObj );

        for( pEntry = Tcl_FirstHashEntry( &local.archiveHash, &sSearch );
                pEntry; pEntry = Tcl_NextHashEntry( &sSearch ) )
        {
            pArchive = Tcl_GetHashValue(pEntry);
            if( Tcl_StringCaseMatch( Tcl_GetString(pArchive->zMountPoint),
                        zPattern, NOCASE_PATHS ) ) {
                Tcl_ListObjAppendElement( NULL, result,
                        Tcl_DuplicateObj(pArchive->zMountPoint) );
            }
        }

        Tcl_DecrRefCount(patternObj);

        return TCL_OK;
    }

    if( !(pFile = GetCrapvfsFile(pathPtr)) ) {
        Tcl_SetStringObj( Tcl_GetObjResult(interp), "stale file handle", -1 );
        return TCL_ERROR;
    }

    if( !pattern ) {
        /* If pattern is null, Tcl is actually just checking to
         * see if this file exists in our filesystem.  Check to
         * make sure the path matches any type data and then
         * append it to the result and return.
         */
        if( CrapvfsFileMatchesType( pFile, types ) ) {
            Tcl_ListObjAppendElement( NULL, result, pathPtr );
        }
        return TCL_OK;
    }

    /* We've determined that the requested path is in our filesystem,
     * so now we want to walk through the children of the directory
     * and find any that match the given pattern and type.  Any we
     * find will be appended to the result.
     */

    for( pEntry = Tcl_FirstHashEntry(&pFile->children, &sSearch);
        pEntry; pEntry = Tcl_NextHashEntry(&sSearch) )
    {
        char *zName;
        pFile = Tcl_GetHashValue(pEntry);
        zName = Tcl_GetString(pFile->zName);

        if( CrapvfsFileMatchesType( pFile, types )
                && Tcl_StringCaseMatch(zName, pattern, NOCASE_PATHS) ) {
            Tcl_ListObjAppendElement( NULL, result,
                    Tcl_FSJoinToPath(pathPtr, 1, &pFile->zName ) );
        }
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ListVolumes --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Called when Tcl is looking for a list of open volumes
 * 	for our filesystem.  The mountpoint for each open archive
 * 	is appended to a list object.
 *
 * Results:
 * 	A Tcl_Obj with 0 refCount
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ListVolumes(void)
{
    Tcl_HashEntry *pEntry;       /* Hash table entry */
    Tcl_HashSearch zSearch;      /* Search all mount points */
    CrapvfsArchive *pArchive;    /* The CRAP archive being mounted */
    Tcl_Obj *pVols = Tcl_NewObj();
  
    for( pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
            pEntry; pEntry = Tcl_NextHashEntry(&zSearch) )
    {
        pArchive = Tcl_GetHashValue(pEntry);

        Tcl_ListObjAppendElement( NULL, pVols,
                Tcl_DuplicateObj(pArchive->zMountPoint) );
    }

    return pVols;
}

/*
 *----------------------------------------------------------------------
 *
 * FileAttrStrings --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Return an array of strings for all of the possible
 * 	attributes for a file in crapvfs.
 *
 * Results:
 * 	Pointer to CrapvfsAttrs
 *
 *----------------------------------------------------------------------
 */

static const char *const *
FileAttrStrings( Tcl_Obj *pathPtr, Tcl_Obj** objPtrRef )
{
    return CrapvfsAttrs;
}

/*
 *----------------------------------------------------------------------
 *
 * FileAttrsGet --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Called for a "file attributes" command from Tcl
 * 	to return the attributes for a file in our filesystem.
 *
 * 	objPtrRef will point to a 0 refCount Tcl_Obj on success.
 *
 * Results:
 * 	Standard Tcl result
 *
 *----------------------------------------------------------------------
 */

static int
FileAttrsGet( Tcl_Interp *interp, int index,
    Tcl_Obj *pathPtr, Tcl_Obj **objPtrRef )
{
    char *zFilename;
    CrapvfsFile *pFile;
    zFilename = Tcl_GetString(pathPtr);

    if( !(pFile = GetCrapvfsFile(pathPtr)) ) {
        return TCL_ERROR;
    }

    switch(index) {
    case CRAPVFS_ATTR_ARCHIVE:
        *objPtrRef= Tcl_DuplicateObj(pFile->pArchive->zName);
        return TCL_OK;
    case CRAPVFS_ATTR_COMPSIZE:
        *objPtrRef=Tcl_NewIntObj(pFile->nByteCompr);
        return TCL_OK;
    case CRAPVFS_ATTR_METHOD:
        if( pFile->method == CRAP_METHOD_NONE ) {
            *objPtrRef= Tcl_NewStringObj( "none", -1 );
            break;
        } else if( pFile->method == CRAP_METHOD_ZLIB ) {
            *objPtrRef= Tcl_NewStringObj( "zlib", -1 );
            break;
        } else if( pFile->method == CRAP_METHOD_LZMA ) {
            *objPtrRef= Tcl_NewStringObj( "lzma", -1 );
            break;
        } else {
            *objPtrRef= Tcl_NewStringObj( "unknown", -1 );
            break;
        }

        return TCL_OK;
    case CRAPVFS_ATTR_MOUNT:
        *objPtrRef= Tcl_DuplicateObj(pFile->pArchive->zMountPoint);
        return TCL_OK;
    case CRAPVFS_ATTR_OFFSET:
        *objPtrRef= Tcl_NewIntObj(pFile->offset);
        return TCL_OK;
    case CRAPVFS_ATTR_UNCOMPSIZE:
        *objPtrRef= Tcl_NewIntObj(pFile->nByte);
        return TCL_OK;
    default:
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FileAttrsSet --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Called to set the value of an attribute for the
 * 	given file.  Since we're a read-only filesystem, this
 * 	always returns an error.
 *
 * Results:
 * 	Returns TCL_ERROR
 *
 *----------------------------------------------------------------------
 */

static int
FileAttrsSet( Tcl_Interp *interp, int index,
                            Tcl_Obj *pathPtr, Tcl_Obj *objPtr )
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Chdir --
 *
 * 	Part of the "crapvfs" Tcl_Filesystem.
 * 	Handles a chdir() call for the filesystem.  Tcl has
 * 	already determined that the directory belongs to us,
 * 	so we just need to check and make sure that the path
 * 	is actually a directory in our filesystem and not a
 * 	regular file.
 *
 * Results:
 * 	0 on success, -1 on failure.
 *
 *----------------------------------------------------------------------
 */

static int
Chdir( Tcl_Obj *pathPtr )
{
    CrapvfsFile *zFile = GetCrapvfsFile(pathPtr);
    if( !zFile || !zFile->isdir ) return -1;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * MountObjCmd --
 *
 * 	This function implements the [crapvfs::mount] command.
 *
 * 	crapvfs::mount ?options? ?crapFile? ?mountPoint?
 *
 * 	Creates a new mount point to the given crap archive.
 * 	All files in the crap archive will be added to the
 * 	virtual filesystem and be available to Tcl as regular
 * 	files and directories.
 *
 * Results:
 * 	Standard Tcl result
 *
 *----------------------------------------------------------------------
 */

static int
MountObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    if( objc != 3 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "?filename? ?mountPoint?" );
        return TCL_ERROR;
    }

    return Crapvfs_Mount(interp,Tcl_GetString(objv[1]),Tcl_GetString(objv[2]));
}

/*
 *----------------------------------------------------------------------
 *
 * UnmountObjCmd --
 *
 * 	This function implements the [crapvfs::unmount] command.
 *
 * 	crapvfs::unmount mountPoint
 *
 * 	Unmount the given mountPoint if it is mounted in our
 * 	filesystem.
 *
 * Results:
 * 	0 on success, -1 on failure.
 *
 *----------------------------------------------------------------------
 */

static int
UnmountObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    if( objc != 2 ) {
        Tcl_WrongNumArgs( interp, objc, objv, "mountPoint" );
        return TCL_ERROR;
    }

    return Crapvfs_Unmount( interp, Tcl_GetString(objv[1]) );
}

static int
SetPasswordObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int keylen;

    if( objc != 2 ) {
        Tcl_WrongNumArgs( interp, objc, objv, "password" );
        return TCL_ERROR;
    }

    if( local.passwordObj != NULL ) {
        Tcl_DecrRefCount( local.passwordObj );
        local.passwordObj = NULL;
    }

    Tcl_GetByteArrayFromObj(objv[1], &keylen);

    if( keylen == 0 ) {
        return TCL_OK;
    }

    local.passwordObj = Tcl_DuplicateObj( objv[1] );
    Tcl_IncrRefCount( local.passwordObj );

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Crapvfs_Init, Crapvfs_SafeInit --
 *
 * 	Initialize the crapvfs package.
 *
 * 	Safe interpreters do not receive the ability to mount and
 * 	unmount crap files.
 *
 * Results:
 * 	Standard Tcl result
 *
 *----------------------------------------------------------------------
 */

int
Crapvfs_SafeInit( Tcl_Interp *interp )
{
#ifdef USE_TCL_STUBS
    if( Tcl_InitStubs( interp, "8.0", 0 ) == TCL_ERROR ) return TCL_ERROR;
#endif

    if( !local.isInit ) {
        /* Register the filesystem and initialize the hash tables. */
	Tcl_FSRegister( 0, &crapvfsFilesystem );
	Tcl_InitHashTable( &local.fileHash, TCL_STRING_KEYS );
	Tcl_InitHashTable( &local.archiveHash, TCL_STRING_KEYS );

	local.isInit = 1;
    }

    Tcl_PkgProvide( interp, "crapvfs", "1.0" );

    return TCL_OK;
}

int
Crapvfs_Init( Tcl_Interp *interp )
{
    Tcl_Obj *name, *value;
    Tcl_Obj *array = Tcl_NewStringObj("::crapvfs::info", -1);

    if( Crapvfs_SafeInit( interp ) == TCL_ERROR ) return TCL_ERROR;

    Tcl_CreateObjCommand(interp, "crapvfs::setPassword", SetPasswordObjCmd,0,0);

    if( !Tcl_IsSafe(interp) ) {
        Tcl_CreateObjCommand(interp, "crapvfs::mount", MountObjCmd, 0, 0);
        Tcl_CreateObjCommand(interp, "crapvfs::unmount", UnmountObjCmd, 0, 0);
    }

    name  = Tcl_NewStringObj("maxFilename", -1);
    value = Tcl_NewIntObj(CRAP_MAX_FILENAME);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("fileHeaderSize", -1);
    value = Tcl_NewIntObj(CRAP_FILE_HEADER_SIZE);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("centralHeaderSize", -1);
    value = Tcl_NewIntObj(CRAP_CENTRAL_HEADER_SIZE);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    return TCL_OK;
}
