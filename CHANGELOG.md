# Installkit changelog

## Unreleased (1.3.1)

### Fixes
- [Windows] Fix truncation of command line arguments

### Changes (potential incompatibilities)
- [Windows] Tcl/Tk has been updated from version 8.5.7 to 8.6b1.2 as it is for other platforms. All standard tcl packages (http, thread, msgcat etc.) have also been updated from tcl8.6b1.2.
- [Windows] ffidl library has been updated from version 0.6 to version 0.8
- [Windows] installkit was built by MinGW GCC 11.4.0 and packed by UPX 4.2.2
- [Windows] added a compatibility element to executable manifest for Windows versions from Vista to Server 2022
- [Linux-x86] installkit was built by GCC 4.4.7 on Centos6.10 with GLIBC 2.12
- [Linux-x86_64] installkit was built by GCC 4.4.7 on Centos6.10 with GLIBC 2.12. Previous linked GLIBC version was 2.3 and GCC version 3.3.2.
- [MacOS-X] installkit was built by clang 900.0.39.2
- [MacOS-X] use TkAqua instead of deprecated Carbon
- [MacOS-X] the minimum version of MacOS is 10.5. The previous minimum version of MacOS was 10.4.
- [MacOS-X] don't use executable packer for this platform
- [FreeBSD-7-x86] don't built
- [MacOS-X-ppc] don't built
- [Solaris-i86pc] don't built
- [Solaris-sparc] don't built
- [Solaris-x86] don't built
- [Linux-*,Windows] update tkdnd package from version 1.0.0 to 2.9.4. Now it's really working!

### Internal changes
- Added a few make recipes for testing
