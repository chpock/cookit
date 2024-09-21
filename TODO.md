cookit:
    * Add PE header options IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP and
      IMAGE_FILE_NET_RUN_FROM_SWAP on Windows to not depend on network when
      running executable. It looks like we have to read/parse/save PE header
      from Tcl and correct header CRC.
      More info: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_file_header
cookfs:
    * add support for pre-analyzing files before adding.
      - check the first 64kb (or whole file if it is small) of file with
        the fastest compression method. If the compression ratio is less
        than 5%, consider that this file is already compressed and add it as
        a single page without queue to the small file buffer.
      - if it is possible, analyze original or destination file name for
        common file types (*.enc, *.tcl/*.tm, *.exe/*.dll/*.so)
      - analyze most common file types: images (png/jpg)/exe/archives by
        signature, select appropriate compression (or no compression)
      - for small files buffer - group files by type and put them as one page.
        As an result, *.enc files will be in one page and *.tcl files in
        another.
      - add ability to compression procedures to "check compression". For new
        file, all compression engines will be tried and a winner will be
        selected.
twapi:
    * try to split into modules
    * remove huge and potentially unused modules like tls/crypt
