### Testing ####

Run cookit tests:
```
$ make check-cookit VERBOSE=1
```

Run specific test only:
```
$ make check-cookit VERBOSE=1 CHECKFLAGS="-file cookit-windows.test -match cookit::Windows-1"
```

Rebuild and run tests:
```
$ rm -f work/stamp-cookit && make cookit && make check-cookit
```
