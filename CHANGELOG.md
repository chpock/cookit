# Installkit changelog

## Unreleased (1.4.0)

### Changes (potential incompatibilities)
- Update Tcl/Tk from version 8.6b1.2 to 8.6.14 for all platforms
- Remove Itcl package. It was broken in IJ 1.3.0. It is only used to convert projects of versions 1.1.0.0 and below.
- Update thread package to version 2.8.9 (from 2.6.6)
- [MacOS-X] the minimum version of MacOS is 10.6 because that is the minimum MacOS version for Tcl/Tk 8.6.8+. The previous minimum version of MacOS was 10.5.
- [MacOS-X] add tkdnd package for this platform
- Update Tktable package to version 2.11.1 (from 2.10.0)
- Update zlib to version 1.3.1
- [Windows] update twapi package to version 4.7.2 (from 1.1.5)

### Internal changes
- VFS engine was migrated to tclvfs+CookFS

## 1.3.1 - 2024-02-19

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
