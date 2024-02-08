#define TAR_BLOCK_SIZE   512
#define TAR_MAX_FILENAME 255

/* POSIX tar Header Block, from POSIX 1003.1-1990  */
typedef struct TarHeader
{
    char name[100];               /*   0-99 */
    char mode[8];                 /* 100-107 */
    char uid[8];                  /* 108-115 */
    char gid[8];                  /* 116-123 */
    char size[12];                /* 124-135 */
    char mtime[12];               /* 136-147 */
    char chksum[8];               /* 148-155 */
    char typeflag;                /* 156-156 */
    char linkname[100];           /* 157-256 */
    char magic[6];                /* 257-262 */
    char version[2];              /* 263-264 */
    char uname[32];               /* 265-296 */
    char gname[32];               /* 297-328 */
    char devmajor[8];             /* 329-336 */
    char devminor[8];             /* 337-344 */
    char prefix[155];             /* 345-499 */
    char padding[12];             /* 500-512 */
} TarHeader;

enum TarFileType 
{
    TAR_REGTYPE  = '0',           /* regular file */
    TAR_REGTYPE0 = '\0',          /* regular file */
    TAR_LNKTYPE  = '1',           /* hard link */
    TAR_SYMTYPE  = '2',           /* symbolic link */
    TAR_CHRTYPE  = '3',           /* character special */
    TAR_BLKTYPE  = '4',           /* block special */
    TAR_DIRTYPE  = '5',           /* directory */
    TAR_FIFOTYPE = '6',           /* FIFO special */
    TAR_CONTTYPE = '7'            /* reserved */
};
