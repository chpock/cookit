### Testing ####

Run installkit tests:
```
$ make check-installkit VERBOSE=1
```

Run specific test only:
```
$ make check-installkit VERBOSE=1 CHECKFLAGS="-file installkit-windows.test -match installkit::Windows-1"
```

Rebuild and run tests:
```
$ rm -f work/stamp-installkit && make installkit && make check-installkit
```
