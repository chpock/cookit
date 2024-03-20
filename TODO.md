cookfs:
    * add support for zstd.
    * add support for option for compression engine. Currently there is only
      hardcoded level 9 compression for zlib.
    * add support for pre-analyzing files before adding.
      - analyze most common file types: images (png/jpg)/exe/archives by
        signature, select appropriate compression (or no compression)
      - add ability to compression procedures to "check compression". For new
        file, all compression engines will be tried and a winner will be
        selected.
      - for new file, try to compress the first 64Kb (or whole file if it is
        small). If all compression engines are not efficient enough, then add
        that file to pages without compression and without smallfilefilter.
twapi:
    * try to split into modules
    * remove huge and potentially unused modules like tls/crypt
