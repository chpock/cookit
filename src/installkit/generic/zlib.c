#include <tcl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <zlib.h>

int Installkit_ZlibCmd(
    ClientData clientData,
    Tcl_Interp*interp,
    int objc,
    Tcl_Obj *CONST objv[])
{
    int e = TCL_OK, idx, dlen, wbits = -MAX_WBITS;
    long flag;
    Byte *data;
    z_stream stream;
    Tcl_Obj *obj = Tcl_GetObjResult(interp);

    const char* cmds[] = {
        "adler32", "crc32", "compress", "deflate", "decompress", "inflate", NULL
    };

    enum {
        ZLIB_ADLER32, ZLIB_CRC32, ZLIB_COMPRESS, ZLIB_DEFLATE,
        ZLIB_DECOMPRESS, ZLIB_INFLATE
    };

    if ( objc < 3 ) {
        Tcl_WrongNumArgs(interp, 1, objv, "option data ?...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], cmds, "option", 0, &idx) != TCL_OK)
        return TCL_ERROR;

    if( idx != 6 ) {
        if( objc > 3 && Tcl_GetLongFromObj(interp, objv[3], &flag) != TCL_OK ) {
            return TCL_ERROR;
        }
        data = Tcl_GetByteArrayFromObj(objv[2], &dlen);
    }

    switch (idx) {
    case ZLIB_ADLER32: /* adler32 str ?start? -> checksum */
        if (objc < 4) {
            flag = (long) adler32(0, 0, 0);
        }
        Tcl_SetLongObj(obj, (long) adler32((uLong) flag, data, dlen));
        return TCL_OK;

    case ZLIB_CRC32: /* crc32 str ?start? -> checksum */
        if (objc < 4) {
            flag = (long) crc32(0, 0, 0);
        }
        Tcl_SetLongObj(obj, (long) crc32((uLong) flag, data, dlen));
        return TCL_OK;

    case ZLIB_COMPRESS: /* compress data ?level? -> data */
        wbits = MAX_WBITS;
    case ZLIB_DEFLATE: /* deflate data ?level? -> data */
        if (objc < 4) {
            flag = Z_DEFAULT_COMPRESSION;
        }

        stream.avail_in = (uInt) dlen;
        stream.next_in = data;

        stream.avail_out = (uInt) dlen + dlen / 1000 + 12;
        Tcl_SetByteArrayLength(obj, stream.avail_out);
        stream.next_out = Tcl_GetByteArrayFromObj(obj, NULL);

        stream.zalloc = 0;
        stream.zfree = 0;
        stream.opaque = 0;

        e = deflateInit2(&stream, (int) flag, Z_DEFLATED, wbits,
        MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        if (e != Z_OK) break;

        e = deflate(&stream, Z_FINISH);
        if (e != Z_STREAM_END) {
            deflateEnd(&stream);
            if (e == Z_OK) {
                e = Z_BUF_ERROR;
            }
        } else {
            e = deflateEnd(&stream);
        }
        break;

    case ZLIB_DECOMPRESS: /* decompress data ?bufsize? -> data */
        wbits = MAX_WBITS;
    case ZLIB_INFLATE: /* inflate data ?bufsize? -> data */
        {
            if (objc < 4) {
                flag = 16 * 1024;
            }

            for (;;) {
                stream.zalloc = 0;
                stream.zfree = 0;

                /* +1 because ZLIB can "over-request" input (but ignore it) */
                stream.avail_in = (uInt) dlen +  1;
                stream.next_in = data;

                stream.avail_out = (uInt) flag;
                Tcl_SetByteArrayLength(obj, stream.avail_out);
                stream.next_out = Tcl_GetByteArrayFromObj(obj, NULL);

                /* Negative value suppresses ZLIB header */
                e = inflateInit2(&stream, wbits);
                if (e == Z_OK) {
                    e = inflate(&stream, Z_FINISH);
                    if (e != Z_STREAM_END) {
                        inflateEnd(&stream);
                        if (e == Z_OK) {
                            e = Z_BUF_ERROR;
                        }
                    } else {
                        e = inflateEnd(&stream);
                    }

                    if (e == Z_OK || e != Z_BUF_ERROR) break;

                    Tcl_SetByteArrayLength(obj, 0);
                    flag *= 2;
                }
            }
            break;
        }
    }

    if (e != Z_OK) {
        Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
        return TCL_ERROR;
    }

    Tcl_SetByteArrayLength(obj, stream.total_out);
    return TCL_OK;
}

int
Zlib_Init( Tcl_Interp *interp )
{
    Tcl_CreateObjCommand( interp, "zlib", Installkit_ZlibCmd, 0, 0);
    Tcl_PkgProvide( interp, "zlib", "1.0" );
    return TCL_OK;
}
