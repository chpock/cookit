#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.

#-----------------------------------------------------------------------
# Sample configure.ac for Tcl Extensions.  The only places you should
# need to modify this file are marked by the string __CHANGE__
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
# This will also define a special symbol for Windows (BUILD_<PACKAGE_NAME>
# so that we create the export library with the dll.
#-----------------------------------------------------------------------

AC_INIT([installkit], [m4_esyscmd_s([cat ./../../version])])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

TEA_INIT()

AC_CONFIG_AUX_DIR(tclconfig)

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

# Tk is statically linked only on Windows
if test "${TEA_PLATFORM}" = "windows" ; then
    TEA_PATH_TKCONFIG
    TEA_LOAD_TKCONFIG
fi

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC and a few others to create the basic setup
# necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

TEA_ADD_SOURCES([main.c])
TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([-I. -I\"`${CYGPATH} ${srcdir}/generic`\"])
TEA_ADD_LIBS([])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([library/jammerTheme.tcl library/installkit.tcl library/wzipvfs.tcl])

#--------------------------------------------------------------------
# __CHANGE__
#
# You can add more files to clean if your extension creates any extra
# files by extending CLEANFILES.
# Add pkgIndex.tcl if it is generated in the Makefile instead of ./configure
# and change Makefile.in to move it from CONFIG_CLEAN_FILES to BINARIES var.
#
# A few miscellaneous platform-specific items:
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

AC_DEFUN([IK_ADD_BOOTSTRAP_FILES], [
    vars="$@"
    for i in $vars; do
        BOOTSTRAP_FILES="$BOOTSTRAP_FILES $i"
    done
    AC_SUBST(BOOTSTRAP_FILES)
])

AC_DEFUN([IK_ADD_BOOTSTRAP_FILES_FROM], [
    dir="$1"
    vars="$2"
    for i in $vars; do
        IK_ADD_BOOTSTRAP_FILES([${dir}/$i])
    done
])

AS_IF([test "${TEA_PLATFORM}" = "windows"], [
    AC_ARG_WITH(lib-twapi, [ --with-lib-twapi library file for twapi package ], LIB_TWAPI_FILE=${withval})

    AC_MSG_CHECKING([for twapi package library file])
    AS_IF([test -f "${LIB_TWAPI_FILE}"], [
        AC_MSG_RESULT([found ${LIB_TWAPI_FILE}])
        TEA_ADD_LIBS([${LIB_TWAPI_FILE}])
    ], [
        AC_MSG_ERROR([could not find ${LIB_TWAPI_FILE}])
    ])

    AC_MSG_CHECKING([for dyncall library file])
    LIB_DYNCALL_DIR="`dirname ${LIB_TWAPI_FILE}`"
    LIB_DYNCALL_FILE="${LIB_DYNCALL_DIR}/libdyncall_s.lib"
    AS_IF([test -f "${LIB_DYNCALL_FILE}"], [
        AC_MSG_RESULT([found ${LIB_DYNCALL_FILE}])
        TEA_ADD_LIBS([-L"${LIB_DYNCALL_DIR}" libdyncall_s.lib])
    ], [
        AC_MSG_ERROR([could not find ${LIB_DYNCALL_FILE}])
    ])
])

AC_ARG_WITH(lib-thread, [ --with-lib-thread library file for Thread package ], LIB_THREAD_FILE=${withval})
AC_MSG_CHECKING([for Thread package library file])
AS_IF([test "${TCL_THREADS}" = 1], [
    AS_IF([test -f "${LIB_THREAD_FILE}"], [
        AC_MSG_RESULT([found ${LIB_THREAD_FILE}])
        TEA_ADD_LIBS([${LIB_THREAD_FILE}])
    ], [
        AC_MSG_ERROR([could not find ${LIB_THREAD_FILE}])
    ])
], [
    AC_MSG_RESULT([not required for threadless builds])
])

AC_ARG_WITH(lib-vfs, [ --with-lib-vfs library file for VFS package ], LIB_VFS_FILE=${withval})
AC_MSG_CHECKING([for VFS package library file])
AS_IF([test -f "${LIB_VFS_FILE}"], [
    AC_MSG_RESULT([found ${LIB_VFS_FILE}])
    TEA_ADD_LIBS([${LIB_VFS_FILE}])
    LIB_VFS_DIR="`dirname "${LIB_VFS_FILE}"`"
], [
    AC_MSG_ERROR([could not find ${LIB_VFS_FILE}])
])

AC_ARG_WITH(lib-cookfs, [ --with-lib-cookfs library file for CookFS package ], LIB_COOKFS_FILE=${withval})
AC_MSG_CHECKING([for CookFS package library file])
AS_IF([test -f "${LIB_COOKFS_FILE}"], [
    AC_MSG_RESULT([found ${LIB_COOKFS_FILE}])
    TEA_ADD_LIBS([${LIB_COOKFS_FILE}])
    LIB_COOKFS_DIR="`dirname "${LIB_COOKFS_FILE}"`"
], [
    AC_MSG_ERROR([could not find ${LIB_COOKFS_FILE}])
])

IK_ADD_BOOTSTRAP_FILES_FROM([${LIB_VFS_DIR}], [ vfsUtils.tcl vfslib.tcl ])

IK_ADD_BOOTSTRAP_FILES_FROM([${LIB_COOKFS_DIR}], [
    pkgconfig.tcl memchan.tcl readerchannel.tcl \
    vfs.tcl writer.tcl optimize.tcl cookvfs.tcl \
])

AC_ARG_WITH(upx-command, [ --with-upx-command use specified command to pack binary ], UPX_COMMAND=${withval})
AC_MSG_CHECKING([for usable UPX command])
AS_IF([test "x${UPX_COMMAND}" != "x"], [
    AC_MSG_RESULT([yes - ${UPX_COMMAND}])
], [
    AC_MSG_RESULT([no])
    UPX_COMMAND="echo"
])
AC_SUBST(UPX_COMMAND)

AC_ARG_WITH(strip-command, [ --with-strip-command use specified command to strip binary ], STRIP_COMMAND=${withval})
AC_MSG_CHECKING([for usable strip command])
AS_IF([test "x${STRIP_COMMAND}" != "x"], [
    AC_MSG_RESULT([yes - ${STRIP_COMMAND}])
], [
    AC_MSG_RESULT([no])
    STRIP_COMMAND="echo"
])
AC_SUBST(STRIP_COMMAND)

IK_ADD_BOOTSTRAP_FILES([library/bootstrap.tcl])

bin_BINARIES="${PACKAGE_NAME}${EXEEXT} ${PACKAGE_NAME}U${EXEEXT}"
if test "${TEA_PLATFORM}" = "windows" ; then
    TEA_ADD_LIBS([-mwindows -pipe -static-libgcc -municode])
    TEA_ADD_CFLAGS([-DUNICODE -D_UNICODE])
    bin_BINARIES="${bin_BINARIES} ${PACKAGE_NAME}A${EXEEXT} ${PACKAGE_NAME}UA${EXEEXT}"
    # These are CFLAGS and LIBS from twapi
    TEA_ADD_CFLAGS([-DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DPSAPI_VERSION=1])
    # These libs must be after twapi static library
    TEA_ADD_LIBS([
        rpcrt4.lib psapi.lib pdh.lib winmm.lib mpr.lib
        winspool.lib version.lib iphlpapi.lib powrprof.lib
        secur32.lib wtsapi32.lib setupapi.lib uxtheme.lib
        credui.lib crypt32.lib wintrust.lib
    ])
    #TEA_ADD_LIBS([gdi32.lib user32.lib])
    #TEA_ADD_SOURCES([win/winFile.c])
    #TEA_ADD_INCLUDES([-I\"$(${CYGPATH} ${srcdir}/win)\"])
else
    : no-op
    #TEA_ADD_SOURCES([unix/unixFile.c])
    #TEA_ADD_LIBS([-lsuperfly])
fi
AC_SUBST(bin_BINARIES)

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
TEA_PRIVATE_TCL_HEADERS

# Tk is statically linked only on Windows
if test "${TEA_PLATFORM}" = "windows" ; then
    TEA_PUBLIC_TK_HEADERS
    #TEA_PRIVATE_TK_HEADERS
    #TEA_PATH_X
fi

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
# This auto-enables if Tcl was compiled threaded.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

#TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

if test "${TEA_PLATFORM}" = "windows" ; then

    # Add installkit::Windows package
    TEA_ADD_TCL_SOURCES([library/installkit-windows.tcl])

    AC_MSG_CHECKING([for manifest version])
    # Version number for manifest. It must be 4 dotted pair Windows version format.
    MANIFEST_VERSION="${PACKAGE_VERSION}.0"
    AS_IF([[ ! echo "${MANIFEST_VERSION}" | grep -q '^[[:digit:]]\+\.[[:digit:]]\+\.[[:digit:]]\+\.[[:digit:]]\+$' ]], [
        AC_MSG_ERROR([failed, it is not 4-dotted version format: ${MANIFEST_VERSION}])
    ])
    AC_MSG_RESULT([${MANIFEST_VERSION}])
    AC_SUBST(MANIFEST_VERSION)
    # X86|AMD64|ARM64|IA64 for manifest
    AC_MSG_CHECKING([for manifest machine])
    # TEA_CONFIG_CFLAGS is buggy and doesn't define $MACHINE in Cygwin
    #
    case "$do64bit" in
        amd64|x64|yes)
            MACHINE="AMD64"
            ;;
        arm64|aarch64)
            MACHINE="ARM64"
            ;;
        ia64)
            MACHINE="IA64"
            ;;
        *)
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
                #ifndef _WIN64
                    #error 32-bit
                #endif
            ]], [[]])], [tcl_win_64bit=yes], [tcl_win_64bit=no] )
            if test "$tcl_win_64bit" = "yes" ; then
                MACHINE="AMD64"
            else
                # This is the default
                MACHINE="X86"
            fi
            ;;
    esac
    AC_MSG_RESULT([${MACHINE}])
    AC_SUBST(MACHINE)
    AC_CONFIG_FILES([win/installkit.admin.manifest win/installkit.user.manifest])

    # Link with Tk on Windows

    AC_DEFINE(USE_TK_STUBS, 1, [Use Tk stubs])

else

    # at least threading support in XZ requires pthreads
    TEA_ADD_LIBS([-lpthread])

fi

_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -Wl,-Map=conftest.map"
AC_MSG_CHECKING([whether LD supports -Wl,-Map=])
AC_LINK_IFELSE([AC_LANG_PROGRAM([])],[
    AC_MSG_RESULT([yes])
    LDFLAGS="$_LDFLAGS -Wl,-Map=\$@.map"
],[
    AC_MSG_RESULT([no])
    LDFLAGS="$_LDFLAGS"
])

_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_MSG_CHECKING([whether CC supports -Wall])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
    AC_MSG_RESULT([yes])
],[
    AC_MSG_RESULT([no])
    CFLAGS="$_CFLAGS"
])

_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wextra"
AC_MSG_CHECKING([whether CC supports -Wextra])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
    AC_MSG_RESULT([yes])
],[
    AC_MSG_RESULT([no])
    CFLAGS="$_CFLAGS"
])

_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -pedantic"
AC_MSG_CHECKING([whether CC supports -pedantic])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
    AC_MSG_RESULT([yes])
],[
    AC_MSG_RESULT([no])
    CFLAGS="$_CFLAGS"
])

#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Determine the name of the tclsh and/or wish executables in the
# Tcl and Tk build directories or the location they were installed
# into. These paths are used to support running test cases only,
# the Makefile should not be making use of these paths to generate
# a pkgIndex.tcl file or anything else at extension build time.
#--------------------------------------------------------------------

#TEA_PROG_TCLSH
#TEA_PROG_WISH

#--------------------------------------------------------------------
# Setup a *Config.sh.in configuration file.
#--------------------------------------------------------------------

#TEA_EXPORT_CONFIG([sample])
#AC_SUBST(SAMPLE_VAR)

#--------------------------------------------------------------------
# Specify files to substitute AC variables in. You may alternatively
# have a special pkgIndex.tcl.in or other files which require
# substituting the AC variables in. Include these here.
#--------------------------------------------------------------------

AC_CONFIG_FILES([Makefile pkgIndex.tcl])
#AC_CONFIG_FILES([sampleConfig.sh])

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the files
# specified with AC_CONFIG_FILES.
#--------------------------------------------------------------------

AC_OUTPUT
