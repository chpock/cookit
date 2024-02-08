#define ZIP_MAX_FILENAME   256

#define ZIP_OK             (0)
#define ZIP_EOF            (0)
#define ZIP_ERRNO          (Z_ERRNO)
#define ZIP_PARAMERROR     (-102)
#define ZIP_BADZIPFILE     (-103)
#define ZIP_INTERNALERROR  (-104)

typedef struct
{
    char pk[2];
    char magic1;
    char magic2;
    char magic3;
    char magic4;
    short flags;
    short type;
    unsigned long date;
    int crc;
    int csize;
    int fsize;
    short fnlen;
    short cnlen;
} zip_file_header;

typedef struct
{
    char pk[2];
    char magic1;
    char magic2;
    char magic3;
    char magic4;
    char magic5;
    char magic6;
    short flags;
    short type;
    unsigned long date;
    int crc;
    int csize;
    int fsize;
    short fnlen;
    short magic7;
    short magic8;
    short magic9;
    short magic10;
    short magic11;
    int magic12;
    int offset;
} zip_central_header;
