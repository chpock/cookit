#include <string.h>
#include <stdlib.h>

#define CRAP_METHOD_NONE 0
#define CRAP_METHOD_ZLIB 1
#define CRAP_METHOD_LZMA 2

#define CRAP_MAX_FILENAME        128
#define CRAP_FILE_HEADER_SIZE    256
#define CRAP_CENTRAL_HEADER_SIZE 256

#define CRAP_GARBAGE_CHECK_SIZE  (1 << 14)

typedef struct CrapCentralHeader
{
    char magic[4];
    char version[2];
    char offset[12];
    char chksum[64];
    char password[64];
    char junk[110];
    Tcl_WideInt  fileOffset;
    Tcl_WideInt  centralOffset;
    int  headerVersion;
} CrapCentralHeader;

typedef struct CrapFileHeader
{
    char magic[4];
    char method[2];
    char name[128];
    char size[12];
    char csize[12];
    char mtime[12];
    char corefile[1];
    char encrypted[1];
    char offset[12];
    char junk[72];
} CrapFileHeader;

typedef struct RC4_CTX {
    unsigned char x;
    unsigned char y;
    unsigned char s[256];
} RC4_CTX;

Tcl_Channel CrapOpen(Tcl_Interp *interp, char *filename, char *mode);
int CrapReadCentralHeader(Tcl_Interp *interp, Tcl_Channel chan,
    CrapCentralHeader *central);
int CrapNextFile(Tcl_Interp *interp, Tcl_Channel chan, CrapFileHeader *header);

int rc4_init( Tcl_Obj *keyObj, RC4_CTX *ctx );
int rc4_crypt( RC4_CTX *ctx, unsigned char *data, int nBytes );

#ifdef TCL_WIDE_INT_IS_LONG
#define strtowide(a) (strtoul(a, NULL, 0))
#else
#define strtowide(a) (strtoull(a, NULL, 0))
#endif
