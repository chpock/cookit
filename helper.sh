#!/bin/sh

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
    Linuxx)
        if test -d /lib64; then
            LIBC=`echo /lib64/libc-*|sed -e 's/.*-\([0-9]\)\.\([0-9]\).*/\1\2/'`
        else
            LIBC=`echo /lib/libc-*|sed -e 's/.*-\([0-9]\)\.\([0-9]\).*/\1\2/'`
        fi

        if test "$LIBC" -lt 23; then
            OS=Linux-$LIBC
        fi
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

    if [ "$PLATFORM" = "MacOS-X" ] || [ "$PLATFORM" = "Windows" ]; then
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
esac
