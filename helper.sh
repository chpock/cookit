#!/bin/sh

set -e

OS=`uname -s`

case $OS in
    AIX)
        PLATFORM=AIX-ppc
        ;;
    Darwin)
        OS=MacOS-X
        ;;
    FreeBSD)
        VERSION=`uname -r | cut -f 1 -d .`
        ;;
    HP-UX)
        PLATFORM=HPUX-hppa
        ;;
    IRIX*)
        PLATFORM=IRIX-mips
        ;;
    SunOS)
        OS=Solaris
        ;;
    CYGWIN_NT*|MINGW*)
        EXE=.exe
        PLATFORM=Windows
        ;;
esac

if test -z "$PLATFORM"; then
    if test -z "$MACHINE"; then
        MACHINE=`uname -m`

        case $MACHINE in
            *86_64*)
                MACHINE=x86_64
                ;;
            *86*)
                MACHINE=x86
                ;;
            *sun*)
                MACHINE=sparc
                ;;
            Power*)
                MACHINE=ppc
                ;;
            arm64)
                MACHINE=arm64
                ;;
        esac
    fi

    if [ "$PLATFORM" = "MacOS-X" ]; then
        if [ "$MACHINE" = "x86" ]; then
            unset MACHINE
        fi
    fi

    if [ -n "$MACHINE" ]; then
        if test -n "$VERSION"; then
            PLATFORM=$OS-$VERSION-$MACHINE
        else
            PLATFORM=$OS-$MACHINE
        fi
    fi
fi

TOP=`pwd`
PREFIX=$TOP/work/$PLATFORM/kit

case $1 in
    platform)
        echo $PLATFORM
        ;;

    makedist)
        echo "Don't forget to update the version file before distributing"
        rm -rf $PREFIX/$PLATFORM
        mkdir $PREFIX/$PLATFORM
        cp version $PREFIX/$PLATFORM

        for i in installkit installkit.exe installkitA.exe \
                 installkitU.exe installkitUA.exe
        do
            if test -f "$PREFIX/bin/$i"; then
                cp $PREFIX/bin/$i $PREFIX/$PLATFORM
            fi
        done

        if test -d "$PREFIX/lib/Tktable2.10"; then
            cp -R "$PREFIX/lib/Tktable2.10" $PREFIX/$PLATFORM/tktable
        fi

        if test -d "$PREFIX/lib/tkdnd1.0"; then
            cp -R "$PREFIX/lib/tkdnd1.0" $PREFIX/$PLATFORM/tkdnd
        fi

        cd $PREFIX
        rm -f $PLATFORM.zip
        FILES=`find $PLATFORM -type f`
        $PREFIX/bin/tclsh8.5 $TOP/src/tools/makezip.tcl $PLATFORM.zip $FILES
        rm -rf $PREFIX/$PLATFORM
        mv $PLATFORM.zip $TOP/work
        echo "File is in work/$PLATFORM.zip"
        ;;

    environment)
        echo "# Build environment:"
        echo "# Generated at: `date --iso-8601=seconds 2>/dev/null || date +"%Y-%m-%dT%H:%M:%S%z"`"
        echo
        if command -v cygcheck >/dev/null 2>&1; then
            echo "\$ cygcheck --version"
            cygcheck --version
            echo
            echo "\$ cmd /c ver"
            cmd /c "ver" | tr -d '\r'
            echo
        elif command -v sw_vers >/dev/null 2>&1; then
            echo "\$ sw_vers -productName"
            sw_vers -productName 2>&1
            echo
            echo "\$ sw_vers -productVersion"
            sw_vers -productVersion 2>&1
            echo
            if command -v pkgutil >/dev/null 2>&1; then
                echo
                echo "\$ pkgutil --pkg-info=com.apple.pkg.CLTools_Executables"
                pkgutil --pkg-info=com.apple.pkg.CLTools_Executables 2>&1
                echo
            fi
        elif [ -e /etc/redhat-release ]; then
            echo "\$ cat /etc/redhat-release"
            cat /etc/redhat-release
            echo
        elif [ -e /etc/lsb-release ]; then
            echo "\$ cat /etc/lsb-release"
            cat /etc/lsb-release
            echo
        fi
        echo "\$ uname -a"
        uname -a
        echo
        echo "\$ $CC --version"
        $CC --version 2>&1
        echo
        echo "\$ $CXX --version"
        $CXX --version 2>&1
        echo
        if command -v ldd >/dev/null 2>&1; then
            echo "\$ ldd --version"
            ldd --version 2>&1
            echo
        fi
        ;;

esac
