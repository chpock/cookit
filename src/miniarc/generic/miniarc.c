#include <tclInt.h>
#include <tclPort.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <zlib.h>

#include "tar.h"
#include "zip.h"
#include "crap.h"

#include "LzmaC/LzmaEnc.h"

#ifdef __WIN32__
#include <windows.h>
#else
#include <grp.h>
#endif

#define MINIARC_VERSION "0.1"

#define BUFLEN (1<<15)

#ifndef F_OK
#    define F_OK 00
#endif

#define streq(a, b) (a[0] == b[0] && strcmp(a, b) == 0)

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) {  p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

#ifdef __GNUC__
inline
#endif
void
swap( unsigned char *lhs, unsigned char *rhs )
{
    unsigned char t = *lhs;
    *lhs = *rhs;
    *rhs = t;
}

int
rc4_init( Tcl_Obj *keyObj, RC4_CTX *ctx )
{
    const unsigned char *k;
    int n = 0, i = 0, j = 0, keylen;

    k = Tcl_GetByteArrayFromObj(keyObj, &keylen);

    ctx->x = 0;
    ctx->y = 0;

    for (n = 0; n < 256; n++)
        ctx->s[n] = n;

    for (n = 0; n < 256; n++) {
        j = (k[i] + ctx->s[n] + j) % 256;
        swap(&ctx->s[n], &ctx->s[j]);
        i = (i + 1) % keylen;
    }
    
    return TCL_OK;
}

int
rc4_crypt( RC4_CTX *ctx, unsigned char *data, int nBytes )
{
    int i, n;
    unsigned char x, y;

    x = ctx->x;
    y = ctx->y;
    for( n = 0; n < nBytes; ++n )
    {
        x = (x + 1) % 256;
        y = (ctx->s[x] + y) % 256;
        swap(&ctx->s[x], &ctx->s[y]);
        i = (ctx->s[x] + ctx->s[y]) % 256;
        data[n] = data[n] ^ ctx->s[i];
    }
    ctx->x = x;
    ctx->y = y;

    return TCL_OK;
}

static int
MiniarcGzipObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
    int i, index, delete = 0, level = 6, len;
    char *filename, *gzfilename;
    Tcl_Obj *fileObj, *gzFileObj;
    Tcl_Channel chan;
    gzFile gzout;
    char lvl[1], buf[BUFLEN];
    char gzmode[3] = "wb";

    const char *switches[] = {
        "-delete", "-level", (char *)NULL
    };

    enum {
        OPT_DELETE, OPT_LEVEL
    };

    if( objc < 2 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "?options? filename ?gzFilename?" );
        return TCL_ERROR;
    }

    for( i = 1; i < objc; ++i )
    {
        char *opt = Tcl_GetString(objv[i]);
        if( opt[0] != '-' ) break;

	if( Tcl_GetIndexFromObj( interp, objv[i], switches,
                "option", 0, &index ) != TCL_OK ) { return TCL_ERROR; }

	/*
	 * If there's no argument after the switch, give them some help
	 * and return an error.
	 */
	if( ++i == objc ) {
            Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                    "no argument given for ", opt, (char *)NULL );
            return TCL_ERROR;
	}

	switch(index) {
        case OPT_DELETE:
            if( Tcl_GetBooleanFromObj( interp, objv[i], &delete ) == TCL_ERROR )
                return TCL_ERROR;
            break;
	case OPT_LEVEL: /* -level 1-9 */
	    if( Tcl_GetIntFromObj(interp, objv[i], &level) == TCL_ERROR )
		return TCL_ERROR;
	    if( (level < 1 || level > 9) ) {
		Tcl_AppendStringsToObj( resultObj,
			"Bad -level.  Must be between 0 and 9.",
			(char *)NULL );
		return TCL_ERROR;
	    }
	    break;
	}
    }

    sprintf( lvl, "%d", level );
    strcat( gzmode, lvl );

    if( objc - i < 1 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "?options? filename ?gzFilename?" );
        return TCL_ERROR;
    }

    fileObj = objv[i];

    if( objc - i == 2 ) {
        gzFileObj = Tcl_DuplicateObj(objv[i+1]);
    } else {
        gzFileObj = Tcl_DuplicateObj( fileObj );
        Tcl_AppendToObj( gzFileObj, ".gz", -1 );
    }
    Tcl_IncrRefCount(gzFileObj);

    filename   = Tcl_GetString(fileObj);
    gzfilename = Tcl_GetString(gzFileObj);

    if( (chan = Tcl_FSOpenFileChannel( interp, fileObj, "rb", 0 )) == NULL ) {
        Tcl_DecrRefCount(gzFileObj);

        Tcl_AppendStringsToObj( resultObj,
                "cannot open \"", filename, "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    if( (gzout = gzopen( gzfilename, gzmode )) == NULL ) {
        Tcl_DecrRefCount(gzFileObj);
        Tcl_Close( interp, chan );

        Tcl_AppendStringsToObj( resultObj,
                "failed to open \"", gzfilename, "\" for output", (char *)NULL);
        return TCL_ERROR;
    }

    for(;;)
    {
        len = Tcl_Read(chan, buf, sizeof(buf));

        if( len == 0 ) break;

        if( gzwrite( gzout, buf, (unsigned)len ) != len ) {
            Tcl_DecrRefCount(gzFileObj);
            Tcl_Close( interp, chan );
            gzclose(gzout);

            Tcl_SetStringObj( resultObj, "error writing compressed data", -1 );
            return TCL_ERROR;
        }
    }

    Tcl_Close( interp, chan );
    Tcl_DecrRefCount(gzFileObj);

    if( gzclose(gzout) != Z_OK ) {
        Tcl_SetStringObj( resultObj, "error closing compressed file", -1 );
        return TCL_ERROR;
    }

    if( delete ) {
        Tcl_FSDeleteFile( fileObj );
    }

    return TCL_OK;
}

static int
MiniarcGunzipObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
    int i, index, len, delete = 0, nameLen;
    char *filename, *gzfilename;
    Tcl_Obj *fileObj, *gzFileObj;
    gzFile gzin;
    Tcl_Channel chan;
    char buf[BUFLEN];

    const char *switches[] = {
        "-delete", (char *)NULL
    };

    enum {
        OPT_DELETE
    };

    if( objc < 2 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "?options? gzFilename ?filename?" );
        return TCL_ERROR;
    }

    for( i = 1; i < objc; ++i )
    {
        char *opt = Tcl_GetString(objv[i]);
        if( opt[0] != '-' ) break;

	if( Tcl_GetIndexFromObj( interp, objv[i], switches,
                "option", 0, &index ) != TCL_OK ) { return TCL_ERROR; }

	/*
	 * If there's no argument after the switch, give them some help
	 * and return an error.
	 */
	if( ++i == objc ) {
            Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                    "no argument given for ", opt, (char *)NULL );
            return TCL_ERROR;
	}

	switch(index) {
        case OPT_DELETE:
            if( Tcl_GetBooleanFromObj( interp, objv[i], &delete ) == TCL_ERROR )
                return TCL_ERROR;
            break;
	}
    }

    if( objc - i < 1 ) {
        Tcl_WrongNumArgs( interp, 1, objv, "?options? gzFilename ?filename?" );
        return TCL_ERROR;
    }

    gzFileObj = objv[i];

    gzfilename = Tcl_GetStringFromObj( gzFileObj, &nameLen );

    if( objc - i == 2 ) {
        fileObj = Tcl_DuplicateObj( objv[i+1] );
    } else {
        fileObj = Tcl_DuplicateObj( gzFileObj );
        if( Tcl_StringCaseMatch( gzfilename, "*.gz", 1 ) ) {
            Tcl_SetObjLength( fileObj, nameLen - 3 );
        }
    }
    Tcl_IncrRefCount(fileObj);
    filename = Tcl_GetString(fileObj);

    if( (gzin = gzopen( gzfilename, "rb" )) == NULL ) {
        Tcl_DecrRefCount(fileObj);

        Tcl_AppendStringsToObj( resultObj,
                "cannot open \"", gzfilename, "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    if( (chan = Tcl_FSOpenFileChannel( interp, fileObj, "wb", 644 )) == NULL ) {
        gzclose( gzin );
        Tcl_DecrRefCount(fileObj);

        Tcl_AppendStringsToObj( resultObj,
                "failed to open \"", filename, "\" for output",
                (char *)NULL );
        return TCL_ERROR;
    }

    for(;;)
    {
        len = gzread( gzin, buf, sizeof(buf) );

        if( len < 0 ) {
            gzclose(gzin);
            Tcl_Close( interp, chan );
            Tcl_DecrRefCount(fileObj);

            Tcl_SetStringObj( resultObj, "error reading from file", -1 );
            return TCL_ERROR;
        }

        if( len == 0 ) break;

        if( Tcl_Write( chan, buf, (unsigned)len ) != len ) {
            gzclose(gzin);
            Tcl_Close( interp, chan );
            Tcl_DecrRefCount(fileObj);

            Tcl_SetStringObj( resultObj, "error writing output data", -1 );
            return TCL_ERROR;
        }
    }

    Tcl_Close( interp, chan );
    Tcl_DecrRefCount(fileObj);

    if( gzclose(gzin) != Z_OK ) {
        Tcl_SetStringObj( resultObj, "error closing compressed file", -1 );
        return TCL_ERROR;
    }

    if( delete ) {
        Tcl_FSDeleteFile( gzFileObj );
    }

    return TCL_OK;
}

/* Put an octal string into the specified buffer.
 * The number is zero and space padded and possibly null padded.
 * Return TCL_OK on success or TCL_ERROR on failure.
 */
static int
CopyOctal( Tcl_Interp *interp, char *cp, long value, int len )
{
    int  tempLength;
    char tempBuffer[32];
    char *tempString = tempBuffer;

    /* Create a string of the specified length with an initial space,
     * leading zeroes and the octal number, and a trailing null.  */
    sprintf( tempString, "%0*lo", len - 1, value );

    /* If the string is too large, suppress the leading space.  */
    tempLength = strlen( tempString ) + 1;
    if( tempLength > len ) {
        tempLength--;
        tempString++;
    }

    /* If the string is still too large, suppress the trailing null.  */
    if( tempLength > len ) {
        tempLength--;
    }

    /* If the string is still too large, fail.  */
    if (tempLength > len) {
        if( interp ) {
            Tcl_SetStringObj( Tcl_GetObjResult(interp),
                    "failed to store integer value: octal too long", -1 );
        }
        return TCL_ERROR;
    }

    /* Copy the string to the field.  */
    memcpy( cp, tempString, len );

    return TCL_OK;
}

static int
MiniarcTarAddFileObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    long chksum = 0;
    struct TarHeader header;
    const unsigned char *headerP = (const unsigned char *)&header;
    
    unsigned int uid;
    unsigned int gid;
    mode_t mode;
    time_t mtime;
    int i, index, nameLen;
    char *storeName, *fileName, *channelName;
    char *uname = NULL, *gname = NULL;
    Tcl_Channel chan;
    Tcl_StatBuf statBuf;

    const char *switches[] = {
        "-gid", "-gname", "-mtime", "-name",
        "-permissions", "-uid", "-uname", (char *)NULL
    };

    enum {
        TAR_OPT_GID, TAR_OPT_GNAME, TAR_OPT_MTIME,
        TAR_OPT_NAME, TAR_OPT_PERMISSIONS, TAR_OPT_UID, TAR_OPT_UNAME
    };

    if( objc < 4 ) {
        Tcl_WrongNumArgs( interp, 1, objv,
            "miniarcHandle nativeHandle fileName ?options?" );
        return TCL_ERROR;
    }

    channelName = Tcl_GetString( objv[2] );
    chan = Tcl_GetChannel( interp, channelName, NULL );

    storeName = fileName = Tcl_GetStringFromObj( objv[3], &nameLen );

    if( Tcl_FSStat( objv[3], &statBuf ) == -1 ) {
        Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                "error adding \"", fileName, "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    mode  = statBuf.st_mode;
    uid   = statBuf.st_uid;
    gid   = statBuf.st_gid;
    mtime = statBuf.st_mtime;

    if( objc > 4 ) {
	int optc;
	Tcl_Obj **optv;

	Tcl_ListObjGetElements( interp, objv[4], &optc, &optv );

	for( i = 0; i < optc; ++i )
	{
	    char *opt = Tcl_GetString(optv[i]);
	    if( opt[0] != '-' ) break;

	    if( Tcl_GetIndexFromObj( interp, optv[i], switches,
		    "option", 0, &index ) != TCL_OK ) return TCL_ERROR;

	    /*
	     * If there's no argument after the switch, give them some help
	     * and return an error.
	     */
	    if( ++i == optc ) {
		Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
			"no argument given for ", opt, (char *)NULL );
		return TCL_ERROR;
	    }

	    switch(index) {
	    case TAR_OPT_GID:
		if( Tcl_GetIntFromObj( interp, optv[i], &gid ) == TCL_ERROR )
		    return TCL_ERROR;
		break;
	    case TAR_OPT_GNAME:
		gname = Tcl_GetString(optv[i]);
		break;
	    case TAR_OPT_MTIME:
		if( Tcl_GetLongFromObj( interp, optv[i], &mtime ) == TCL_ERROR )
		    return TCL_ERROR;
		break;
	    case TAR_OPT_NAME:
		storeName = Tcl_GetStringFromObj( optv[i], &nameLen );
                if( nameLen == 0 ) {
                    storeName = fileName;
                }
		break;
	    case TAR_OPT_PERMISSIONS:
		if( Tcl_GetIntFromObj( interp, optv[i], (int *)&mode )
		    == TCL_ERROR ) return TCL_ERROR;
		break;
	    case TAR_OPT_UID:
		if( Tcl_GetIntFromObj( interp, optv[i], &uid ) == TCL_ERROR )
		    return TCL_ERROR;
		break;
	    case TAR_OPT_UNAME:
		uname = Tcl_GetString(optv[i]);
		break;
	    }
	}
    }

    /* Strip off the leading slash (and drive name in Windows). */
#ifdef __WIN32__
    if( storeName[1] == ':' ) storeName += 2;
#endif
    if( storeName[0] == '/' ) ++storeName;

    if( nameLen > TAR_MAX_FILENAME ) {
        Tcl_SetStringObj( Tcl_GetObjResult(interp), "filename too long", -1 );
        return TCL_ERROR;
    }

    memset( &header, 0, TAR_BLOCK_SIZE );

    if( nameLen > sizeof(header.name) ) {
        /* File name is too long.  We need to split it up
         * into prefix and suffix to store it.
         */
        int n   = nameLen;
        char *p = &storeName[--n];
        for(; n > 0; --n, --p )
        {
            if( *p == '/' ) break;
        }

        strncpy( header.name, ++p, n + 1 ); 
        strncpy( header.prefix, storeName, n ); 
    } else {
        strncpy( header.name, storeName, sizeof(header.name) ); 
    }

    CopyOctal( interp, header.mode, mode, sizeof(header.mode) );
    CopyOctal( interp, header.uid, uid, sizeof(header.uid) );
    CopyOctal( interp, header.gid, gid, sizeof(header.gid) );
    CopyOctal( interp, header.size, 0, sizeof(header.size) );
    CopyOctal( interp, header.mtime, mtime, sizeof(header.mtime) );

    /* Magic UNIX Standard Tar identifier. */
    strncpy( header.magic, "ustar", 6 );
    strncpy( header.version, "  ",  2 );

    if( uname ) {
        strncpy( header.uname, uname, sizeof(header.uname) );
    } else {
#ifdef __WIN32__
        strcpy( header.uname, "root" );
#else
        struct passwd *passwd;
        if( (passwd = getpwuid(statBuf.st_uid)) ) {
            strncpy( header.uname, passwd->pw_name, sizeof(header.uname) );
        }
#endif
    }

    if( gname ) {
        strncpy( header.gname, gname, sizeof(header.gname) );
    } else {
#ifdef __WIN32__
        strcpy( header.gname, "root" );
#else
        struct group *grp;
        if( (grp = getgrgid(statBuf.st_gid)) ) {
            strncpy( header.gname, grp->gr_name, sizeof(header.gname) );
        }
#endif
    }

    if( S_ISREG(statBuf.st_mode) ) {
        /* Regular file */
        header.typeflag  = TAR_REGTYPE;
        CopyOctal( interp, header.size, statBuf.st_size, sizeof(header.size) );
    } else if( S_ISDIR(statBuf.st_mode) ) {
        /* Directory */
        header.typeflag  = TAR_DIRTYPE;
        strncat( header.name, "/", sizeof(header.name) ); 
    } else if( S_ISLNK(statBuf.st_mode) ) {
        Tcl_Obj *linkObj;
        header.typeflag  = TAR_SYMTYPE;
        if( !(linkObj = Tcl_FSLink( objv[3], NULL, 0 )) ) {
            return TCL_ERROR;
        }
        strncpy(header.linkname,Tcl_GetString(linkObj),sizeof(header.linkname));
    } else if( S_ISCHR(statBuf.st_mode) ) {
        /* Not handled */
    } else if( S_ISBLK(statBuf.st_mode) ) {
        /* Not handled */
    } else if( S_ISFIFO(statBuf.st_mode) ) {
        /* Not handled */
    } else {
        Tcl_SetStringObj( Tcl_GetObjResult(interp), "bad file type", -1 );
        return TCL_ERROR;
    }

    /* Calculate the chksum.  Blank out the chksum field for this
     * calculation and then get the sum of all the bytes.
     */
    memset( header.chksum, ' ', sizeof(header.chksum) );

    for( i = 0; i < TAR_BLOCK_SIZE; ++i )
    {
        chksum += *headerP++;
    }
    CopyOctal( interp, header.chksum, chksum, 7 );
    
    /* Now write the header out to disk */
    Tcl_Write( chan, (char *)&header, TAR_BLOCK_SIZE );

    /* If this is a regular file, write out its contents now. */
    if( header.typeflag == TAR_REGTYPE ) {
        char buf[BUFLEN];
        Tcl_Channel inchan;
        int bytesRead = 0, totalBytes = 0;

        if( !(inchan = Tcl_OpenFileChannel( interp, fileName, "rb", 0 )) ) {
            return TCL_ERROR;
        }

        for(;;)
        {
            bytesRead = Tcl_Read(inchan, buf, BUFLEN);

            if( bytesRead > 0 ) {
                totalBytes += bytesRead;
                Tcl_Write( chan, buf, bytesRead );
            } else {
                break;
            }
        }

        /* Pad the file up to TAR_BLOCK_SIZE. */
        for (; (totalBytes % TAR_BLOCK_SIZE); ++totalBytes ) {
            Tcl_Write( chan, "\0", 1 );
        }

        Tcl_Close( interp, inchan );

	Tcl_SetIntObj( Tcl_GetObjResult(interp), totalBytes );
    }

    return TCL_OK;
}

#define LOCALHEADERSIZE    30
#define CENTRALHEADERSIZE  46

#define LOCALHEADERMAGIC   (0x04034b50)
#define CENTRALHEADERMAGIC (0x02014b50)

static unsigned long
GetZipDate( time_t mtime )
{
    unsigned long year;
    struct tm *ptm;

    ptm = localtime(&mtime);

    year = ptm->tm_year;

    if( year > 1980 ) {
        year -= 1980;
    } else if( year > 80 ) {
        year -= 80;
    }

    return (uLong) (((ptm->tm_mday) + (32 * (ptm->tm_mon+1)) + (512 * year))
            << 16) | ((ptm->tm_sec/2) + (32* ptm->tm_min)
                + (2048 * (uLong)ptm->tm_hour));
}

static void
SetByte( char *dest, unsigned long data, int size )
{
    int i;
    unsigned char *buf = (unsigned char *)dest;

    for( i = 0; i < size; ++i )
    {
        buf[i] = (unsigned char)(data & 0xff);
        data >>= 8;
    }
}

static int
MiniarcZipAddFileObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    time_t mtime;
    int index, nameLen, headerOffset;
    int level = Z_DEFAULT_COMPRESSION;
    Tcl_Channel chan;
    Tcl_StatBuf statBuf;
    char *miniHandle, *channelName, *storeName, *fileName;
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

    char fileheader[LOCALHEADERSIZE];
    char centralheader[CENTRALHEADERSIZE];

    const char *switches[] = {
        "-corefile", "-level", "-method",
        "-mtime", "-name", "-password", (char *)NULL
    };

    enum {
        CRAP_OPT_COREFILE, CRAP_OPT_LEVEL, CRAP_OPT_METHOD,
        CRAP_OPT_MTIME, CRAP_OPT_NAME, CRAP_OPT_PASSWORD
    };

    if( objc < 4 ) {
        Tcl_WrongNumArgs( interp, 1, objv,
            "miniarcHandle nativeHandle fileName ?options?" );
        return TCL_ERROR;
    }

    miniHandle  = Tcl_GetString( objv[1] );
    channelName = Tcl_GetString( objv[2] );
    chan = Tcl_GetChannel( interp, channelName, NULL );

    storeName = fileName = Tcl_GetStringFromObj( objv[3], &nameLen );

    if( Tcl_FSStat( objv[3], &statBuf ) == -1 ) {
        Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                "error adding \"", fileName, "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    mtime = statBuf.st_mtime;

    if( objc > 4 ) {
	int i, optc;
	Tcl_Obj **optv;

	Tcl_ListObjGetElements( interp, objv[4], &optc, &optv );

	for( i = 0; i < optc; ++i )
	{
            char *opt = Tcl_GetString(optv[i]);
            if( opt[0] != '-' ) break;

            if( Tcl_GetIndexFromObj( interp, optv[i], switches,
                    "option", 0, &index ) != TCL_OK ) return TCL_ERROR;

            /*
             * If there's no argument after the switch, give them some help
             * and return an error.
             */
            if( ++i == optc ) {
                Tcl_AppendStringsToObj( resultObj,
                        "no argument given for ", opt, (char *)NULL );
                return TCL_ERROR;
            }

            switch(index) {
            case CRAP_OPT_COREFILE:
                break;
            case CRAP_OPT_LEVEL:
                if( Tcl_GetIntFromObj( interp, optv[i], &level ) == TCL_ERROR )
                    return TCL_ERROR;
                if( level < 1 || level > 9 ) {
                    Tcl_SetStringObj( Tcl_GetObjResult(interp),
                        "invalid level: must be between 1 and 9", -1 );
                    return TCL_ERROR;
                }
                break;
            case CRAP_OPT_METHOD:
                break;
            case CRAP_OPT_MTIME:
                if( Tcl_GetLongFromObj( interp, optv[i], &mtime ) == TCL_ERROR )
                    return TCL_ERROR;
                break;
            case CRAP_OPT_NAME:
                storeName = Tcl_GetStringFromObj( optv[i], &nameLen );
                if( nameLen == 0 ) {
                    storeName = fileName;
                }
                break;
            case CRAP_OPT_PASSWORD:
                break;
            }
        }
    }

    mtime = GetZipDate( mtime );

    headerOffset = Tcl_Tell(chan);

    SetByte( fileheader, LOCALHEADERMAGIC, 4 ); /* magic */
    SetByte( fileheader+4, 20, 2 ); /* magic */
    SetByte( fileheader+6, 0, 2 ); /* flags */
    SetByte( fileheader+8, 8, 2 ); /* method */
    SetByte( fileheader+10, mtime, 4 ); /* date time */
    SetByte( fileheader+14, 0, 4 ); /* crc */
    SetByte( fileheader+18, 0, 4 ); /* compressed size */
    SetByte( fileheader+22, statBuf.st_size, 4 ); /* uncompressed size */
    SetByte( fileheader+26, nameLen, 2 ); /* name length */
    SetByte( fileheader+28, 0, 2 ); /* extra length */

    SetByte( centralheader, CENTRALHEADERMAGIC, 4 ); /* magic */
    SetByte( centralheader+4, 0, 2 ); /* version made by */
    SetByte( centralheader+6, 20, 2 ); /* magic */
    SetByte( centralheader+8, 0, 2 ); /* flags */
    SetByte( centralheader+10, 8, 2 ); /* method */
    SetByte( centralheader+12, mtime, 4 ); /* date time */
    SetByte( centralheader+16, 0, 4 ); /* crc */
    SetByte( centralheader+20, 0, 4 ); /* compressed size */
    SetByte( centralheader+24, statBuf.st_size, 4 ); /* uncompressed size */
    SetByte( centralheader+28, nameLen, 2 ); /* name length */
    SetByte( centralheader+30, 0, 2 ); /* extra length */
    SetByte( centralheader+32, 0, 2 ); /* comment length */
    SetByte( centralheader+34, 0, 2 ); /* disk number */
    SetByte( centralheader+36, 0, 2 ); /* ?? */
    SetByte( centralheader+38, 0, 4 ); /* ?? */
    SetByte( centralheader+42, headerOffset, 4 ); /* byte offset */

    /* Write out our header. */
    Tcl_Write( chan, fileheader, LOCALHEADERSIZE );

    Tcl_Write( chan, storeName, nameLen );

    {
        z_stream z;
        int crc = 0, err = ZIP_OK;
        char *inbuf, *outbuf;
        int totalBytes = 0;
        int footerOffset;
        Tcl_Channel inchan;
        Tcl_Obj *nameObj;

        if( !(inchan = Tcl_OpenFileChannel( interp, fileName, "rb", 0 )) ) {
            return TCL_ERROR;
        }

        inbuf  = (char *)Tcl_Alloc( BUFLEN );
        outbuf = (char *)Tcl_Alloc( BUFLEN );

        z.zalloc = Z_NULL;
        z.zfree  = Z_NULL;
        z.opaque = Z_NULL;

        z.avail_in  = 0;
        z.next_in   = inbuf;
        z.avail_out = BUFLEN;
        z.next_out  = outbuf;

        err = deflateInit2(&z, level, Z_DEFLATED, -MAX_WBITS,
                            MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);

        for (;;)
        {
            if( z.avail_in == 0 ) {
                z.next_in = inbuf;
                z.avail_in = Tcl_Read(inchan, inbuf, BUFLEN);
                crc = crc32( crc, inbuf, z.avail_in );
            }
            if( z.avail_in == 0 ) break;

            err = deflate( &z, Z_NO_FLUSH );

            if( err != Z_OK && err != Z_STREAM_END ) {
                Tcl_AppendStringsToObj( resultObj,
                    "unexpected zlib deflate error: ", z.msg, (char *)NULL);
                Tcl_Free( inbuf );
                Tcl_Free( outbuf );
                return TCL_ERROR;
            }

            if( z.avail_out == 0 ) {
                totalBytes += Tcl_Write( chan, outbuf, BUFLEN );
                z.avail_out = BUFLEN;
                z.next_out  = outbuf;
            }
        }
        
        do {
            unsigned count = 0;
            err = deflate( &z, Z_FINISH );
            count = BUFLEN - z.avail_out;
            if( count ) {
                totalBytes += Tcl_Write( chan, outbuf, count );
                z.avail_out = BUFLEN;
                z.next_out  = outbuf;
            }
        } while( err == Z_OK );

        deflateEnd( &z );

	Tcl_Free(inbuf);
	Tcl_Free(outbuf);
        Tcl_Close( interp, inchan );

        footerOffset = Tcl_Tell(chan);

        /* Seek back and write out the full header. */
        Tcl_Seek( chan, headerOffset, SEEK_SET );

        SetByte( fileheader+14, crc, 4 ); /* crc */
        SetByte( fileheader+18, totalBytes, 4 ); /* compressed size */

        Tcl_Write( chan, fileheader, LOCALHEADERSIZE );

        SetByte( centralheader+16, crc, 4 ); /* crc */
        SetByte( centralheader+20, totalBytes, 4 ); /* compressed size */

        nameObj = Tcl_NewStringObj( "", 0 );
        Tcl_AppendStringsToObj( nameObj, "::miniarc::", miniHandle, ":info",
                                (char *)NULL );

        Tcl_ObjSetVar2( interp, nameObj, Tcl_NewStringObj( "headers", -1 ),
                Tcl_NewByteArrayObj( centralheader, CENTRALHEADERSIZE ),
                TCL_APPEND_VALUE );
        Tcl_ObjSetVar2( interp, nameObj, Tcl_NewStringObj( "headers", -1 ),
                Tcl_NewStringObj( storeName, nameLen ), TCL_APPEND_VALUE );

        Tcl_Seek( chan, footerOffset, SEEK_SET );

        Tcl_SetIntObj( resultObj, totalBytes );
    }

    return TCL_OK;
}

typedef struct {
    ISeqInStream funcTable;
    Tcl_Channel chan;
} InStream;

static SRes
LzmaRead(void *p, void *buf, size_t *size)
{
    InStream *stream = (InStream *)p;

    if (*size == 0) return SZ_OK;
    *size = Tcl_Read(stream->chan, buf, *size);
    return SZ_OK;
}

typedef struct {
    ISeqOutStream funcTable;
    Tcl_Channel chan;
} OutStream;

static size_t
LzmaWrite(void *pp, const void *buf, size_t size)
{
    return Tcl_Write(((OutStream *)pp)->chan, (char *)buf, size);
}

static int
MiniarcCrapAddObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    struct CrapFileHeader header;
    
    time_t mtime;
    int i, encrypt = 0, corefile = 0, nameLen, index;
    int level = Z_DEFAULT_COMPRESSION;
    int method = (int)clientData;
    Tcl_Channel chan;
    char *miniHandle, *channelName, *storeName, *fileName;
    Tcl_StatBuf statBuf;
    RC4_CTX ctx;
    Tcl_Obj *nameObj;
    Tcl_Channel inchan;
    Tcl_WideInt totalSize = 0, totalCsize = 0;

    const char *switches[] = {
        "-corefile", "-level", "-method",
        "-mtime", "-name", "-password", (char *)NULL
    };

    enum {
        CRAP_OPT_COREFILE, CRAP_OPT_LEVEL, CRAP_OPT_METHOD,
        CRAP_OPT_MTIME, CRAP_OPT_NAME, CRAP_OPT_PASSWORD
    };

    if( objc < 4 ) {
        Tcl_WrongNumArgs( interp, 1, objv,
            "miniarcHandle nativeHandle fileName ?options?" );
        return TCL_ERROR;
    }

    miniHandle  = Tcl_GetString( objv[1] );
    channelName = Tcl_GetString( objv[2] );
    chan = Tcl_GetChannel( interp, channelName, NULL );

    storeName = fileName = Tcl_GetStringFromObj( objv[3], &nameLen );

    if( Tcl_FSStat( objv[3], &statBuf ) == -1 ) {
        Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                "error adding \"", fileName, "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    mtime = statBuf.st_mtime;
    totalSize = statBuf.st_size;

    if( objc > 4 ) {
	int optc;
	Tcl_Obj **optv;

	Tcl_ListObjGetElements( interp, objv[4], &optc, &optv );

	for( i = 0; i < optc; ++i )
	{
            char *opt = Tcl_GetString(optv[i]);
            if( opt[0] != '-' ) break;

            if( Tcl_GetIndexFromObj( interp, optv[i], switches,
                    "option", 0, &index ) != TCL_OK ) return TCL_ERROR;

            /*
             * If there's no argument after the switch, give them some help
             * and return an error.
             */
            if( ++i == optc ) {
                Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                        "no argument given for ", opt, (char *)NULL );
                return TCL_ERROR;
            }

            switch(index) {
            case CRAP_OPT_COREFILE:
                if( Tcl_GetBooleanFromObj( interp, optv[i], &corefile )
                    == TCL_ERROR ) return TCL_ERROR;
                break;
            case CRAP_OPT_LEVEL:
                if(Tcl_GetIntFromObj( interp, optv[i], &level)
                    == TCL_ERROR ) return TCL_ERROR;
                if (level < 0 || level > 9) {
                    Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                            "invalid level \"", level, "\": must be an "
                            "integer between 0 and 9", (char *)NULL);
                }
                break;
            case CRAP_OPT_METHOD:
                break;
            case CRAP_OPT_MTIME:
                if( Tcl_GetLongFromObj( interp, optv[i], &mtime ) == TCL_ERROR )
                    return TCL_ERROR;
                break;
            case CRAP_OPT_NAME:
                storeName = Tcl_GetStringFromObj( optv[i], &nameLen );
                if( nameLen == 0 ) {
                    storeName = fileName;
                }
                break;
            case CRAP_OPT_PASSWORD:
                encrypt = 1;
                rc4_init( optv[i], &ctx );
                break;
            }
        }
    }

    if (corefile) encrypt = 0;
    if (level == 0) method = CRAP_METHOD_NONE;

    /* Strip off the leading slash (and drive name in Windows). */
#ifdef __WIN32__
    if( storeName[1] == ':' ) storeName += 2;
#endif
    if( storeName[0] == '/' ) ++storeName;

    if( nameLen > CRAP_MAX_FILENAME ) {
        Tcl_SetStringObj( Tcl_GetObjResult(interp), "filename too long", -1 );
        return TCL_ERROR;
    }

    memset(&header, 0, CRAP_FILE_HEADER_SIZE);
    snprintf(header.offset, 12, "%" TCL_LL_MODIFIER "d", Tcl_Tell(chan));

    if (method == CRAP_METHOD_NONE) {
        char buf[BUFLEN];
        Tcl_WideInt bytesRead = 0;

        if( !(inchan = Tcl_OpenFileChannel( interp, fileName, "rb", 0 )) ) {
            return TCL_ERROR;
        }

        for(;;) {
            bytesRead = Tcl_Read( inchan, buf, BUFLEN);
            if(bytesRead <= 0) break;

            if(encrypt) {
                rc4_crypt( &ctx, buf, bytesRead );
            }
            totalCsize += Tcl_Write(chan, buf, bytesRead);
        }

        strncpy(header.method, "00", sizeof(header.method));
    } else if (method == CRAP_METHOD_ZLIB) {
        z_stream z;
        int err = ZIP_OK;
        char *inbuf, *outbuf;

        if( !(inchan = Tcl_OpenFileChannel( interp, fileName, "rb", 0 )) ) {
            return TCL_ERROR;
        }

        inbuf  = (char *)Tcl_Alloc( BUFLEN );
        outbuf = (char *)Tcl_Alloc( BUFLEN );

        z.zalloc = Z_NULL;
        z.zfree  = Z_NULL;
        z.opaque = Z_NULL;

        z.avail_in  = 0;
        z.next_in   = inbuf;
        z.avail_out = BUFLEN;
        z.next_out  = outbuf;

        err = deflateInit2(&z, level, Z_DEFLATED, -MAX_WBITS,
                            MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);

        for (;;)
        {
            if( z.avail_in == 0 ) {
                z.next_in = inbuf;
                z.avail_in = Tcl_Read(inchan, inbuf, BUFLEN);
            }
            if( z.avail_in == 0 ) break;

            err = deflate( &z, Z_NO_FLUSH );

            if( err != Z_OK && err != Z_STREAM_END ) {
                Tcl_AppendStringsToObj( Tcl_GetObjResult(interp),
                    "unexpected zlib deflate error: ", z.msg, (char *)NULL);
                Tcl_Free( inbuf );
                Tcl_Free( outbuf );
                return TCL_ERROR;
            }

            if(z.avail_out == 0) {
                if(encrypt ) {
                    rc4_crypt( &ctx, outbuf, BUFLEN );
                }
                totalCsize += Tcl_Write( chan, outbuf, BUFLEN );
                z.avail_out = BUFLEN;
                z.next_out  = outbuf;
            }
        }
        
        do {
            unsigned count = 0;
            err = deflate( &z, Z_FINISH );
            count = BUFLEN - z.avail_out;
            if(count) {
                if(encrypt) {
                    rc4_crypt( &ctx, outbuf, count );
                }
                totalCsize += Tcl_Write( chan, outbuf, count );
                z.avail_out = BUFLEN;
                z.next_out  = outbuf;
            }
        } while(err == Z_OK);

        deflateEnd( &z );

	Tcl_Free(inbuf);
	Tcl_Free(outbuf);

        strncpy(header.method, "ZL", sizeof(header.method));
    } else if (method == CRAP_METHOD_LZMA) {
        SRes res;
        Tcl_WideInt iPos;
        InStream inStream;
        OutStream outStream;
        CLzmaEncHandle enc;
        CLzmaEncProps props;
        LzByte lzHeader[LZMA_PROPS_SIZE];
        size_t headerSize = LZMA_PROPS_SIZE;

        if ((enc = LzmaEnc_Create(&g_Alloc)) == 0) {
            Tcl_SetStringObj(Tcl_GetObjResult(interp), "ALLOC FAILED", -1);
            return TCL_ERROR;
        }

        LzmaEncProps_Init(&props);
        res = LzmaEnc_SetProps(enc, &props);
        if (res != SZ_OK) {
            Tcl_SetStringObj(Tcl_GetObjResult(interp), "PROPS FAILED", -1);
            return TCL_ERROR;
        }

        iPos = Tcl_Tell(chan);
        res = LzmaEnc_WriteProperties(enc, lzHeader, &headerSize);
        if (res != SZ_OK) {
            Tcl_SetStringObj(Tcl_GetObjResult(interp), "WRITE PROPS FAILED",-1);
            return TCL_ERROR;
        }

        /* Write out the properties. */
        totalCsize += Tcl_Write(chan, lzHeader, headerSize);

        if( !(inchan = Tcl_OpenFileChannel( interp, fileName, "rb", 0 )) ) {
            return TCL_ERROR;
        }

        inStream.funcTable.Read = LzmaRead;
        inStream.chan = inchan;
        outStream.funcTable.Write = LzmaWrite;
        outStream.chan = chan;

        res = LzmaEnc_Encode(enc, &outStream.funcTable, &inStream.funcTable,
            NULL, &g_Alloc, &g_Alloc);

        LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);

        strncpy(header.method, "LZ", sizeof(header.method));

        totalCsize += Tcl_Tell(chan) - iPos;
    }

    strncpy(header.magic, "FILE", sizeof(header.magic));
    strncpy(header.name, storeName, sizeof(header.name));
    snprintf(header.mtime, 12, "%ld", mtime);
    snprintf(header.size, 12, "%" TCL_LL_MODIFIER "d", totalSize);
    snprintf(header.csize, 12, "%" TCL_LL_MODIFIER "d", totalCsize);

    if (corefile) {
        strncpy(header.corefile, "C", sizeof(header.corefile));
    }

    if (encrypt) {
        strncpy(header.encrypted, "E", sizeof(header.encrypted));
    }

    Tcl_Close(interp, inchan);

    nameObj = Tcl_NewObj();
    Tcl_AppendStringsToObj(nameObj, "::miniarc::", miniHandle, ":info",
                            (char *)NULL);

    Tcl_ObjSetVar2(interp, nameObj, Tcl_NewStringObj("headers", -1),
            Tcl_NewStringObj((char *)&header, CRAP_FILE_HEADER_SIZE),
            TCL_APPEND_VALUE);

    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), totalCsize);

    return TCL_OK;
}

static int
MiniarcCrapCryptFileObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int res;
    Tcl_WideInt offset;
    Tcl_WideInt bytesToRead, bytesRead, bytesLeft, iPos;
    char buf[BUFLEN];
    CrapFileHeader header;
    CrapCentralHeader central;
    Tcl_Channel chan;
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

    if( objc != 4 ) {
        Tcl_WrongNumArgs( interp, 1, objv,
                "filename password encryptedPassword" );
        return TCL_ERROR;
    }

    if( (chan = Tcl_FSOpenFileChannel( interp, objv[1], "rb+", 0 )) == NULL ) {
        Tcl_AppendStringsToObj( resultObj,
                "cannot open \"", Tcl_GetString(objv[1]),
                "\": no such file or directory",
                (char *)NULL );
        return TCL_ERROR;
    }

    if (CrapReadCentralHeader(interp, chan, &central) == TCL_ERROR) {
        return TCL_ERROR;
    }

    while ((res = CrapNextFile(interp, chan, &header)) == TCL_OK) {
        RC4_CTX ctx;

        if( header.corefile[0] == 'C' ) {
            /* We don't encrypt core files. */
            continue;
        }

        if( header.encrypted[0] == 'E' ) {
            /* We don't encrypt already-encrypted files. */
            continue;
        }

        Tcl_Seek( chan, -CRAP_FILE_HEADER_SIZE, SEEK_CUR );
        strncpy(header.encrypted, "E", sizeof(header.encrypted));
        Tcl_Write( chan, (char *)&header, CRAP_FILE_HEADER_SIZE );

        rc4_init( objv[2], &ctx );
        offset = strtowide(header.offset);
        bytesLeft = strtowide(header.csize);
        iPos = Tcl_Tell(chan);
        Tcl_Seek(chan, offset, SEEK_SET);

        for(; bytesLeft > 0;) {
            Tcl_WideInt pos = Tcl_Tell(chan);

            bytesToRead = BUFLEN;
            if( bytesToRead > bytesLeft ) {
                bytesToRead = bytesLeft;
            }

            bytesRead = Tcl_Read(chan, buf, bytesToRead);
            bytesLeft -= bytesRead;
            rc4_crypt(&ctx, buf, bytesRead);
            Tcl_Seek(chan, pos, SEEK_SET);
            Tcl_Write(chan, buf, bytesRead);
        }

        Tcl_Seek(chan, iPos, SEEK_SET);
    }
    
    strncpy(central.password, Tcl_GetString(objv[3]), sizeof(central.password));

    Tcl_Write( chan, (char *)&central, CRAP_CENTRAL_HEADER_SIZE );

    Tcl_Close( interp, chan );

    return TCL_OK;
}

static int
MiniarcCrapInfoObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST objv[]
) {
    int res;
    Tcl_WideInt offset = 0;
    int version = 0;
    int encrypted = 0;
    int fileCount = 0;
    Tcl_WideInt dataOffset = -1;
    int saveHeaders = 1;
    Tcl_WideInt coreFileOffset = -1;
    Tcl_WideInt fileHeaderOffset = -1;
    Tcl_WideInt centralHeaderOffset = -1;
    CrapFileHeader header;
    CrapCentralHeader central;
    Tcl_Channel chan = NULL;
    Tcl_Obj *name, *value, *array;
    Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

    if (objc < 3 || objc > 4) {
        Tcl_WrongNumArgs( interp, objc, objv, "?-handle? filename arrayName" );
        return TCL_ERROR;
    }

    if (objc == 3) {
        char *fileName = Tcl_GetString(objv[1]);
        if (!(chan = Tcl_OpenFileChannel(interp, fileName, "r", 0))) {
            return TCL_ERROR;
        }
        array = objv[2];
    } else {
        int mode;
        char *chanName = Tcl_GetString(objv[2]);
        if (!(chan = Tcl_GetChannel(interp, chanName, &mode))) {
            return TCL_ERROR;
        }
        offset = Tcl_Tell(chan);
        array = objv[3];
    }

    if(Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK) {
        Tcl_Close(interp, chan);
        return TCL_ERROR;
    }

    if (CrapReadCentralHeader(interp, chan, &central) == TCL_ERROR) {
        Tcl_SetBooleanObj(resultObj, 0);
        goto done;
    }

    version = central.headerVersion;
    fileHeaderOffset = central.fileOffset;
    centralHeaderOffset = central.centralOffset;

    name = Tcl_NewStringObj("coreHeaders", -1);

    while((res = CrapNextFile(interp, chan, &header)) == TCL_OK) {
        ++fileCount;

        if (dataOffset == -1) {
            dataOffset = strtowide(header.offset);
        }

        if( header.encrypted[0] == 'E' ) {
            encrypted = 1;
        }

        if (saveHeaders) {
            value = Tcl_NewStringObj((char *)&header, CRAP_FILE_HEADER_SIZE);
            Tcl_ObjSetVar2(interp, array, name, value, TCL_APPEND_VALUE);
        }

        if (saveHeaders && streq(header.name, "boot.tcl")) {
            saveHeaders = 0;
            coreFileOffset = strtowide(header.offset) + strtowide(header.csize);
        }
    }

    if (res == TCL_ERROR) {
        Tcl_SetBooleanObj(resultObj, 0);
        goto done;
    }

    name  = Tcl_NewStringObj("version", -1);
    value = Tcl_NewIntObj(version);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("centralOffset", -1);
    value = Tcl_NewWideIntObj(centralHeaderOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("headerOffset", -1);
    value = Tcl_NewWideIntObj(fileHeaderOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("dataOffset", -1);
    value = Tcl_NewWideIntObj(dataOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("coreOffset", -1);
    value = Tcl_NewWideIntObj(coreFileOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("encrypted", -1);
    value = Tcl_NewIntObj(encrypted);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("encryptedPassword", -1);
    value = Tcl_NewStringObj(central.password, 64);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("checksum", -1);
    value = Tcl_NewStringObj(central.chksum, -1);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("nFiles", -1);
    value = Tcl_NewIntObj(fileCount);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("extraSize", -1);
    value = Tcl_NewIntObj(dataOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("dataSize", -1);
    value = Tcl_NewIntObj(fileHeaderOffset - dataOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("headerSize", -1);
    value = Tcl_NewIntObj(centralHeaderOffset - fileHeaderOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    name  = Tcl_NewStringObj("fullDataSize", -1);
    value = Tcl_NewWideIntObj(fileHeaderOffset);
    Tcl_ObjSetVar2(interp, array, name, value, 0);

    Tcl_SetBooleanObj(resultObj, 1);
done:
    if (chan) {
        if (objc == 3) {
            Tcl_Close(interp, chan);
        } else {
            Tcl_Seek(chan, offset, SEEK_SET);
        }
    }
    return TCL_OK;
}

extern Sha1_Init( Tcl_Interp *interp );

int
Miniarc_Init( Tcl_Interp *interp )
{

#ifdef USE_TCL_STUBS
    if( !Tcl_InitStubs( interp, "8.4", 0 ) ) {
        return TCL_ERROR;
    }
#endif

    Sha1_Init(interp);

    Tcl_CreateObjCommand(interp, "miniarc::gzip",
        MiniarcGzipObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "miniarc::gunzip",
        MiniarcGunzipObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "miniarc::zip::addfile",
            MiniarcZipAddFileObjCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "miniarc::tar::addfile",
            MiniarcTarAddFileObjCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "miniarc::crap::fileinfo",
            MiniarcCrapInfoObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "miniarc::crap::cryptFile",
            MiniarcCrapCryptFileObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "miniarc::crap::none::addfile",
            MiniarcCrapAddObjCmd, (ClientData)CRAP_METHOD_NONE, 0);
    Tcl_CreateObjCommand(interp, "miniarc::crap::zlib::addfile",
            MiniarcCrapAddObjCmd, (ClientData)CRAP_METHOD_ZLIB, 0);
    Tcl_CreateObjCommand(interp, "miniarc::crap::lzma::addfile",
            MiniarcCrapAddObjCmd, (ClientData)CRAP_METHOD_LZMA, 0);

    Tcl_PkgProvide(interp, "miniarc", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::tar", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::zip", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::crap", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::crap::none", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::crap::zlib", MINIARC_VERSION);
    Tcl_PkgProvide(interp, "miniarc::crap::lzma", MINIARC_VERSION);

    return TCL_OK;
}
