#include <tcl.h>
#include "crap.h"

Tcl_Channel
CrapOpen(Tcl_Interp *interp, char *filename, char *mode)
{
    Tcl_Channel chan;

    if (!(chan = Tcl_OpenFileChannel(interp, filename, mode, 0))) {
        return NULL;
    }

    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")!=TCL_OK) {
        Tcl_Close(interp, chan);
        return NULL;
    }

    return chan;
}

int
CrapReadCentralHeader(Tcl_Interp *interp, Tcl_Channel chan,
    CrapCentralHeader *central)
{
    Tcl_Obj *obj;
    Tcl_WideInt pos = 0;
    CrapFileHeader header;

    Tcl_Seek(chan, 0, SEEK_SET);
    Tcl_Read(chan, (char *)central, CRAP_CENTRAL_HEADER_SIZE);
    if (memcmp(central->magic, "CRAP", 4) == 0) {
        central->centralOffset = pos;
        pos = Tcl_Tell(chan);
    } else {
        pos = Tcl_Seek(chan, -CRAP_CENTRAL_HEADER_SIZE, SEEK_END);
        Tcl_Read(chan, (char *)central, CRAP_CENTRAL_HEADER_SIZE);
        if (memcmp(central->magic, "CRAP", 4)) {
            int i = 0;
            char buf[CRAP_GARBAGE_CHECK_SIZE];
            char *p = buf;

            pos = Tcl_Seek(chan, -CRAP_GARBAGE_CHECK_SIZE, SEEK_END);
            Tcl_Read(chan, buf, CRAP_GARBAGE_CHECK_SIZE);

            while (i < CRAP_GARBAGE_CHECK_SIZE) {
                if (*p == 'C' && memcmp(p, "CRAP02", 6) == 0) {
                    pos += i;
                    Tcl_Seek(chan, pos, SEEK_SET);
                    Tcl_Read(chan, (char *)central, CRAP_CENTRAL_HEADER_SIZE);
                    break;
                }
                ++i;
                ++p;
            }

            if (i >= CRAP_GARBAGE_CHECK_SIZE) {
                if (interp) {
                    Tcl_SetStringObj(Tcl_GetObjResult(interp),
                        "bad end of central directory record", -1);
                }
                return TCL_ERROR;
            }
        }

        central->centralOffset = pos;
        obj = Tcl_NewStringObj(central->offset, 12);
        Tcl_GetWideIntFromObj(interp, obj, &pos);
        Tcl_DecrRefCount(obj);
    }

    obj = Tcl_NewStringObj(central->version, 2);
    Tcl_GetIntFromObj(interp, obj, &central->headerVersion);
    Tcl_DecrRefCount(obj);

    Tcl_Seek(chan, pos, SEEK_SET);
    Tcl_Read(chan, (char *)&header, CRAP_FILE_HEADER_SIZE);
    if (memcmp(header.magic, "FILE", 4) != 0) pos = 0;
    Tcl_Seek(chan, pos, SEEK_SET);
    central->fileOffset = pos;

    return TCL_OK;
}

int
CrapNextFile(Tcl_Interp *interp, Tcl_Channel chan, CrapFileHeader *header)
{
    Tcl_Read(chan, (char *)header, CRAP_FILE_HEADER_SIZE);

    if (memcmp(header->magic, "CRAP", 4) == 0) {
        return TCL_BREAK;
    }

    if (memcmp(header->magic, "FILE", 4) == 0) {
        return TCL_OK;
    }
    
    if (interp) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp),
            "bad central file record", -1);
    }

    return TCL_ERROR;
}
