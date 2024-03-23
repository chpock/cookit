cookfs:
    * add support for zstd.
    * add support for option for compression engine. Currently there is only
      hardcoded level 9 compression for zlib/xz.
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
