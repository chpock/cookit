#!/bin/sh

set -e

if [ -z "$PLATFORM" ]; then

    case "$(uname -s)" in
        Darwin) OS=apple-darwin10.6 ;;
        *Linux) OS=pc-linux-gnu ;;
        CYGWIN_NT*|MINGW*)
            EXE=.exe
            OS=w64-mingw32
            ;;
        *)
            echo "Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac

    case "$(uname -m)" in
        *x86_64*) ARCH=x86_64 ;;
        *86*)     ARCH=x86 ;;
        Power*)   ARCH=powerpc ;;
        arm64)    ARCH=arm64 ;;
        *)
            echo "Unsupported ARCH: $(uname -m)"
            exit 1
            ;;
    esac

    PLATFORM="${ARCH}-${OS}"

fi

TOP=`pwd`
PREFIX=$TOP/work/$PLATFORM/kit

case $1 in
    platform)
        echo $PLATFORM
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
