#!/bin/sh

USE_SCI_LINUX6_DEV_TOOLSET=1

set -e

SELF_HOME="$(cd "$(dirname "$0")"; pwd)"
SELF_FILE="$SELF_HOME/$(basename "$0")"
BUILD_HOME="$(pwd)"

if [ "$SELF_HOME" = "$BUILD_HOME" ]; then
    mkdir -p "$SELF_HOME"/build
    cd "$SELF_HOME"/build
    BUILD_HOME="$SELF_HOME"/build
fi

if uname -r | grep -q -- '-WSL2'; then
    IS_WSL=1
fi

COLORS="33 34 35 36 92 93 94 95 96"

progress() {
    LABEL="$1"
    COLOR="$2"
    while IFS= read -r line; do
        case "$line" in
# Hide known and expected warnings:
#
# /home/vagrant/cookit-source-Linux-x86/deps/cookfs/7zip/C/Sha256.c:445:5: warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
# /home/vagrant/cookit-source-Linux-x86/deps/cookfs/7zip/C/Sha256.c:446:5: warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
# /home/vagrant/cookit-source-Linux-x86/deps/cookfs/7zip/C/Sha1.c:366:5: warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
# /home/vagrant/cookit-source-Linux-x86/deps/cookfs/7zip/C/Sha1.c:367:5: warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
            */cookfs/7zip/*Wstrict-aliasing*) : noop;;
# /Users/vagrant/cookit-source-MacOS-X/deps/cookfs/7zip/C/Threads.c:546:36: warning: unknown warning group '-Watomic-implicit-seq-cst', ignored [-Wunknown-warning-option]
            */cookfs/7zip/*Wunknown-warning-option*) : noop;;
# /home/vagrant/cookit-source-Linux-x86/cookit/tclx/generic/tclXutil.c:358:17: warning: comparison between signed and unsigned integer expressions [-Wsign-compare]
# /home/vagrant/cookit-source-Linux-x86/cookit/tclx/generic/tclXutil.c:994:60: warning: unused parameter 'offType' [-Wunused-parameter]
# /home/vagrant/cookit-source-Linux-x86/cookit/tclx/unix/tclXunixId.c:601:27: warning: unused parameter 'clientData' [-Wunused-parameter]
            */cookit/tclx/*Wunused-parameter*) : noop;;
            */cookit/tclx/*Wsign-compare*) : noop;;
# /Users/vagrant/cookit-source-MacOS-X/deps/tk/unix/../macosx/ttkMacOSXTheme.c:1397:47: warning: incompatible pointer types passing 'SInt *' (aka 'int *') to parameter of type 'SInt32 *' (aka 'long *') [-Wincompatible-pointer-types]
            */tk/unix/*Wincompatible-pointer-types*) : noop;;
# ld: warning: text-based stub file /System/Library/Frameworks//QuartzCore.framework/QuartzCore.tbd and library file /System/Library/Frameworks//QuartzCore.framework/QuartzCore are out of sync. Falling back to library file for linking.
# ld: warning: text-based stub file /System/Library/Frameworks//QuartzCore.framework/QuartzCore.tbd and library file /System/Library/Frameworks//QuartzCore.framework/QuartzCore are out of sync. Falling back to library file for linking.
            *QuartzCore.framework/QuartzCore\ are\ out\ of\ sync*) : noop;;
# <command-line>: warning: "BUILD_vfs" redefined
            *BUILD_vfs*redefined*) : noop;;
# /tmp/work/Linux-x86_64/out/include/tcl.h:2553:55: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
            */include/tcl.h*Wint-to-pointer-cast*) : noop;;
# /w/projects/cookit/deps/twapi/twapi/base/tclobjs.c:4823:16: warning: ‘nenc’ may be used uninitialized [-Wmaybe-uninitialized]
            */src/twapi/twapi/*Wmaybe-uninitialized*) : noop;;
# /w/projects/cookit/deps/twapi/twapi/base/twapi.c:288:9: warning: unused variable ‘compressed’ [-Wunused-variable]
            */src/twapi/twapi/*Wunused-variable*) : noop;;
# /w/projects/cookit/deps/twapi/twapi/etw/etw.c:795:35: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
            */src/twapi/twapi/*Wint-to-pointer-cast*) : noop;;
# /w/projects/cookit/deps/twapi/twapi/etw/etw.c:849:31: warning: cast from pointer to integer of different size [-Wpointer-to-int-cast]
            */src/twapi/twapi/*Wpointer-to-int-cast*) : noop;;
            \[OK\]*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[32m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            *Total*Passed*Skipped*Failed*0*)
                : ignore test results with "Failed: 0"
                ;;
            *Total*Passed*Skipped*Failed*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[91m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            *Total*0*Passed*Skipped*Failed*0*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[91m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            \[ERROR\]*|Test\ file\ error:*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[91m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            make:\ \*\*\**)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[91m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            make\[*|*Tests\ running\ in\ working\ dir:*|Tests\ located\ in:*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m %s\n' "$COLOR" "$LABEL" "$line"
                ;;
            *\ warning:\ *)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[33m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            \[INFO\]*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m %s\n' "$COLOR" "$LABEL" "$line"
                ;;
        esac
    done
}

getcolor() {
    [ -n "$WORKING_COLORS" ] || WORKING_COLORS="$COLORS"
    CURRENT_COLOR="${WORKING_COLORS%% *}"
    _TMP_COLORS="${WORKING_COLORS#* }"
    [ "$WORKING_COLORS" = "$_TMP_COLORS" ] && unset WORKING_COLORS || WORKING_COLORS="$_TMP_COLORS"
}

if [ "$1" != "build" ] && [ "$1" != "build-local" ]; then

    BUILD_PLATFORMS=
    while true; do
        [ -n "$1" ] || break
        case "$1" in
            --*) break;;
        esac
        [ -z "$BUILD_PLATFORMS" ] || BUILD_PLATFORMS="$BUILD_PLATFORMS "
        BUILD_PLATFORMS="$BUILD_PLATFORMS$1"
        shift
    done

    if [ -z "$BUILD_PLATFORMS" ]; then
        BUILD_PLATFORMS="i686-w64-mingw32 x86_64-w64-mingw32 x86_64-apple-darwin10.6 i686-pc-linux-gnu x86_64-pc-linux-gnu"
        BUILD_ALL="$BUILD_HOME/all"
        rm -rf "$BUILD_ALL" "$BUILD_HOME/cookit"-*
    fi

    # Kill all child processes on ctrl-c or exit
    trap "trap - TERM && echo 'Caught SIGTERM, terminating child processes...' && kill -- -$$" INT TERM EXIT

    for PLATFORM in $BUILD_PLATFORMS; do
        rm -f "$BUILD_HOME/$PLATFORM".*
        rm -rf "$BUILD_HOME/$PLATFORM"
        LOG_OUT="$BUILD_HOME/$PLATFORM.log"
        #{
        #    {
        #        "$SELF_FILE" "$PLATFORM" | tee "$LOG_OUT" | progress out "$PLATFORM"
        #    } 2>&1 1>&3 | tee "$LOG_ERR" | progress err "$PLATFORM"
        #} 3>&1 1>&2 &
        getcolor
        "$SELF_FILE" build "$PLATFORM" "$@" 2>&1 | tee "$LOG_OUT" | progress "$PLATFORM" "$CURRENT_COLOR" &
    done

    wait

    trap - INT TERM EXIT

    # Check that build is successful
    for PLATFORM in $BUILD_PLATFORMS; do
        if [ ! -e "$BUILD_HOME/$PLATFORM.zip" ]; then
            echo "Error: platform '$PLATFORM' failed."
            BUILD_FAILED=1
        fi
    done

    [ -z "$BUILD_FAILED" ] || exit 1

    echo
    if [ -n "$BUILD_ALL" ]; then
        mkdir -p "$BUILD_ALL"
        for PLATFORM in $BUILD_PLATFORMS; do
            cp -r "$BUILD_HOME/$PLATFORM/kit"/* "$BUILD_ALL"
        done
        VERSION="$(cat "$SELF_HOME"/version)"
        PACKAGE="$BUILD_HOME/cookit-$VERSION.tar.gz"
        echo "Create package: $PACKAGE"
        ( cd "$BUILD_ALL" && tar czf "$PACKAGE" * )
    else
        echo "Build has been launched for specific platforms. Cookit package will not be built."
    fi

    exit 0

fi

log() {
    echo "[INFO] $1"
}

ok() {
    echo "[OK] $1"
}

error() {
    echo "[ERROR] $1"
}

logcmd() {
    log "command: '$*'"
    "$@"
}

retry() {
    RETRY_COUNT=0
    while ! "$@"; do
        if [ $RETRY_COUNT -ge 10 ]; then
            error "Retry failed"
            exit 1
        fi
        RETRY_COUNT=$(( RETRY_COUNT + 1 ))
        log "Retry in 5 seconds"
        sleep 5
    done
}

[ "$1" != "build-local" ] || BUILD_LOCAL=1
PLATFORM="$2"
shift
shift

if [ -n "$BUILD_LOCAL" ] || [ "$PLATFORM" = "i686-w64-mingw32" ] || [ "$PLATFORM" = "x86_64-w64-mingw32" ]; then

    log "Start local build..."

    if [ "$PLATFORM" = "i686-pc-linux-gnu" -o "$PLATFORM" = "x86_64-pc-linux-gnu" ]; then
        CENTOS_VER="$(rpm -E %{rhel} 2>/dev/null)"
        if [ "$CENTOS_VER" = "6" -a "$USE_SCI_LINUX6_DEV_TOOLSET" = "1" ]; then
            echo "CentOS 6 detected. Scientific Linux CERN 6 Developer Toolset is enabled."
            if [ ! -f /etc/yum.repos.d/slc6-devtoolset-i386.repo ]; then
                sudo wget -O /etc/yum.repos.d/slc6-devtoolset-i386.repo http://linuxsoft.cern.ch/cern/devtoolset/slc6-devtoolset.repo
                sudo sed -i -e 's/\(\]$\)/-i386\1/' -e 's/\$basearch/i386/' /etc/yum.repos.d/slc6-devtoolset-i386.repo
                sudo rpm --import http://linuxsoft.cern.ch/cern/slc6X/i386/RPM-GPG-KEY-cern
                sudo wget -O /etc/yum.repos.d/slc6-devtoolset-x86_64.repo http://linuxsoft.cern.ch/cern/devtoolset/slc6-devtoolset.repo
                sudo sed -i -e 's/\(\]$\)/-x86_64\1/' -e 's/\$basearch/x86_64/' /etc/yum.repos.d/slc6-devtoolset-x86_64.repo
                sudo rpm --import http://linuxsoft.cern.ch/cern/slc6X/x86_64/RPM-GPG-KEY-cern
            fi
        else
            unset USE_SCI_LINUX6_DEV_TOOLSET
        fi
    fi

    BUILD_DIR="$BUILD_HOME/$PLATFORM"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"/*
    else
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    case "$PLATFORM" in
        *-apple-darwin*)
            # Use PATH for macports
            PATH="/opt/local/bin:/opt/local/libexec/llvm-16/bin:$PATH"
            export PATH
            CC="clang"
            export CC
            CXX="clang"
            export CXX
            ;;
        i686-w64-mingw32)
            if [ -n "$IS_WSL" ]; then
                # WSL env
                # dependencies
                retry sudo apt-get install -y tcl make gcc-mingw-w64-i686 g++-mingw-w64-i686 binutils-mingw-w64-i686
                sudo apt-get install -y ccache 2>/dev/null || true
            else
                # Cygwin env
                # dependencies
                for DEP in mingw64-i686-gcc-core mingw64-i686-gcc-g++; do
                    if [ "$(cygcheck -c -n "$DEP" 2>/dev/null)" = "$DEP" ]; then
                        log "dependency '$DEP' - OK"
                    else
                        error "Error: dependency '$DEP' - not installed"
                        error "Please run: setup-x86_64.exe -q -P $DEP"
                        exit 1
                    fi
                done
            fi
            set -- "$@" --toolchain-prefix "/usr/bin/i686-w64-mingw32-"
            [ "$MAKE_PARALLEL" != "true" ] || _MAKE_PARALLEL="-j8"
            ;;
        x86_64-w64-mingw32)
            if [ -n "$IS_WSL" ]; then
                # WSL env
                # dependencies
                retry sudo apt-get install -y tcl make gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
                sudo apt-get install -y ccache 2>/dev/null || true
            else
                # Cygwin env
                # dependencies
                for DEP in mingw64-x86-64-gcc-core mingw64-x86-64-gcc-g++; do
                    if [ "$(cygcheck -c -n "$DEP" 2>/dev/null)" = "$DEP" ]; then
                        log "dependency '$DEP' - OK"
                    else
                        error "Error: dependency '$DEP' - not installed"
                        error "Please run: setup-x86_64.exe -q -P $DEP"
                        exit 1
                    fi
                done
            fi
            set -- "$@" --toolchain-prefix "/usr/bin/x86_64-w64-mingw32-"
            [ "$MAKE_PARALLEL" != "true" ] || _MAKE_PARALLEL="-j8"
            ;;
        i686-pc-linux-gnu)
            # dependencies
            # libXcursor-devel - for tkdnd
            if [ "$USE_SCI_LINUX6_DEV_TOOLSET" = "1" ]; then
                sudo yum install -y devtoolset-2-gcc.i686 devtoolset-2-gcc-c++.i686 devtoolset-2-binutils.i686
                # the strip utility doesn't use arch-specific prefix there
                set -- "$@" --toolchain-prefix "/opt/rh/devtoolset-2/root/usr/bin/i686-redhat-linux-" \
                    --strip-utility "/opt/rh/devtoolset-2/root/usr/bin/strip"
            fi
            sudo yum install -y libgcc.i686 glibc-devel.i686 libX11-devel.i686 libXext-devel.i686 libXt-devel.i686 libXcursor-devel.i686
            sudo yum install -y ccache 2>/dev/null || true
            ;;
        x86_64-pc-linux-gnu)
            # dependencies
            # libXcursor-devel - for tkdnd
            if [ "$USE_SCI_LINUX6_DEV_TOOLSET" = "1" ]; then
                sudo yum install -y devtoolset-2-gcc.x86_64 devtoolset-2-gcc-c++.x86_64 devtoolset-2-binutils.x86_64
                # the strip utility doesn't use arch-specific prefix there
                set -- "$@" --toolchain-prefix "/opt/rh/devtoolset-2/root/usr/bin/x86_64-redhat-linux-" \
                    --strip-utility "/opt/rh/devtoolset-2/root/usr/bin/strip"
            fi
            sudo yum install -y libgcc.x86_64 glibc-devel.x86_64 libX11-devel.x86_64 libXext-devel.x86_64 libXt-devel.x86_64 libXcursor-devel.x86_64
            sudo yum install -y ccache 2>/dev/null || true
            ;;
    esac
    "$SELF_HOME/configure" "$@" --platform "$PLATFORM"

    if [ "$MAKE_PARALLEL" = "true" ]; then
        [ -n "$_MAKE_PARALLEL" ] && MAKE_PARALLEL="$_MAKE_PARALLEL" || MAKE_PARALLEL="-j2"
        log "Run make with $MAKE_PARALLEL"
    fi

    make $MAKE_PARALLEL
    make $MAKE_PARALLEL test
    make $MAKE_PARALLEL dist

    if [ "$PLATFORM" = "i686-w64-mingw32" ] && [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
        ok "Done."
    elif [ "$PLATFORM" = "x86_64-w64-mingw32" ] && [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
        ok "Done."
    else
        echo "Done."
    fi

    exit 0

fi

log "Start remote build..."

BUILD_DIR="$BUILD_HOME/$PLATFORM"
rm -rf "$BUILD_DIR"

vagrant() {
    PPWD="$PWD"
    cd "$VAGRANT_VM"
    command vagrant --machine-readable "$@" | tr -d '\r' | sed -e 's/^[[:digit:]]\+,[^,]*,//' -e 's/\([^,]\+\),/\1 /'
    cd "$PPWD"
}

vagrant_ssh() {
    if [ -z "$VAGRANT_KEY" ]; then
        V_INFO="$(vagrant ssh-config | grep '^  ' | sed 's/^[[:space:]]\+//')"
        V_HOST="$(echo "$V_INFO" | grep -m 1 '^HostName ' | awk '{print $2}')"
        # In WSL set VM hostname as Windows host, which is the default routing gateway in WSL.
        [ -z "$IS_WSL" ] || V_HOST="$(ip route | grep default | awk '{print $3}')"
        V_PORT="$(echo "$V_INFO" | grep -m 1 '^Port ' | awk '{print $2}')"
        V_USER="$(echo "$V_INFO" | grep -m 1 '^User ' | awk '{print $2}')"
        V_KEY="$(echo "$V_INFO" | grep '^IdentityFile ' | \
            sed 's/^[^[:space:]]\+[[:space:]]\+//' | \
            tr '\n' '\t' | \
            sed -e 's/\t$//' -e 's/\t/ -i /g' -e 's/^/-i /' \
        )"
        V_OPTS="$(echo "$V_INFO" | \
            grep -E '^(PubkeyAcceptedKeyTypes|HostKeyAlgorithms|UserKnownHostsFile)' | \
            sed "s/^\([^[:space:]]\+\)[[:space:]]\+\(.*\)/\1=\"\2\"/" | \
            tr '\n' '\t' | \
            sed -e 's/\t$//' -e 's/\t/ -o /g' -e 's/^/-o /'
        )"
        if [ -n "$IS_WSL" ]; then
            V_KEY="$(wslpath -u "$(echo "$V_KEY" | awk '{print $NF}')")"
            cp "$V_KEY" "/tmp/$PLATFORM-ssh.key"
            chmod 0600 "/tmp/$PLATFORM-ssh.key"
            V_KEY="-i /tmp/$PLATFORM-ssh.key"
        fi
        VAGRANT_OPTS="$V_OPTS"
        VAGRANT_KEY="$V_KEY"
        VAGRANT_HOST="$V_USER@$V_HOST"
        VAGRANT_PORT="$V_PORT"
        # In WSL we must use the first forwarded port. The second forwarded port
        # is bound to the loopback interface and is not available for WSL VM.
        if [ -n "$IS_WSL" ]; then
            VAGRANT_PORT="$(vagrant port | grep '^forwarded_port ' | head -n 1 | \
                awk '{print $2}' | cut -d, -f2)"
        fi
    fi
}

vagrant_lock() {
    TIMEOUT=60
    LOCK_TYPE="$1"
    [ -n "$LOCK_TYPE" ] || LOCK_TYPE="soft"
    vagrant_unlock
    if [ "$LOCK_TYPE" = "hard" ]; then
        COUNT=0
        while [ $COUNT -ne $TIMEOUT ] && [ "$(echo "$VAGRANT_VM"/*.hard.lock)" != "$VAGRANT_VM/*.hard.lock" ]; do
            log "[$COUNT/$TIMEOUT] VM is locked. Retry in 10 seconds."
            sleep 10
            COUNT=$(( COUNT + 1 ))
        done
        if [ $COUNT -eq $TIMEOUT ]; then
            error "Error: Timeout reached."
            exit 1
        fi
    fi
    touch "$VAGRANT_VM/$PLATFORM.$LOCK_TYPE.lock"
}

vagrant_unlock() {
    rm -f "$VAGRANT_VM/$PLATFORM".*.lock
}

vagrant_unlocked() {
    if [ "$(echo "$VAGRANT_VM"/*.lock)" = "$VAGRANT_VM/*.lock" ]; then
        return 0
    else
        return 1
    fi
}

VAGRANT_VM="$(cat <<EOF
i386-apple-darwin10.6 /c/DriveD/VM/loc-70-mac-101206-64
x86_64-apple-darwin10.6 /c/DriveD/VM/loc-70-mac-101206-64
i686-pc-linux-gnu /c/DriveD/VM/loc-26-lin-cent6-64
x86_64-pc-linux-gnu /c/DriveD/VM/loc-26-lin-cent6-64
EOF
)"

VAGRANT_VM="$(echo "$VAGRANT_VM" | grep -- "^$PLATFORM " | sed 's/^[^[:space:]]\+[[:space:]]\+//')"

if [ -z "$VAGRANT_VM" ]; then
    log "Error: Unknown platform '$1'" >&2
    exit 1
fi

vagrant_lock hard

unset VM_ACTION

while [ "$STATE" != "running" ]; do
    log "Check VM state..."
    STATE="$(vagrant status | grep '^state ' | awk '{print $2}')"
    log "The VM state is: '$STATE'"
    if [ "$STATE" = "poweroff" ]; then
        if [ -n "$VM_ACTION" ]; then
            error "Error: could not start the VM."
            exit 1
        fi
        log "Start the VM..."
        VM_ACTION="up"
    elif [ "$STATE" = "saved" ]; then
        if [ -z "$VM_ACTION" ]; then
            log "Start the VM..."
            VM_ACTION="up"
        elif [ "$VM_ACTION" = "up" ]; then
            log "Start failed. Try to reload the VM..."
            VM_ACTION="reload"
        else
            error "Error: could not start the VM."
            exit 1
        fi
    elif [ "$STATE" = "gurumeditation" ]; then
        if [ -n "$VM_ACTION" ]; then
            error "Error: could not start the VM."
            exit 1
        fi
        log "VM is meditating. Let's try to reload it..."
        VM_ACTION="reload"
    elif [ -z "$STATE" ]; then
        log "Warning: VM state is empty. Retry."
        unset VM_ACTION
    elif [ "$STATE" != "running" ]; then
        error "Error: unexpected VM state."
        exit 1
    else
        break
    fi
    vagrant "$VM_ACTION" | grep -E '^ui (output|info|error),' | sed 's/^[^,]\+,//' | sed 's/^/[vagrant] /'
done

log "Get SSH config..."
vagrant_ssh
log "Sync sources..."
logcmd rsync -a --exclude '.git' --exclude 'build' --delete -e "ssh -o StrictHostKeyChecking=no $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT" "$SELF_HOME"/* "$VAGRANT_HOST:cookit-source-$PLATFORM"

vagrant_lock soft

log "Start build..."
logcmd ssh $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT -o StrictHostKeyChecking=no "$VAGRANT_HOST" "mkdir -p /tmp/work && cd /tmp/work && MAKE_PARALLEL=\"$MAKE_PARALLEL\" ~/cookit-source-$PLATFORM/build.sh build-local $PLATFORM $@" && R=0 || R=$?
log "Sync build results..."
[ -d "$BUILD_DIR" ] || mkdir -p "$BUILD_DIR"
logcmd rsync -a --delete -e "ssh -o StrictHostKeyChecking=no $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT" "$VAGRANT_HOST:/tmp/work/$PLATFORM/*" "$BUILD_DIR"

vagrant_unlock

if [ "$R" -eq 0 ]; then
    if [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
        if [ -n "$KEEP_VM" ]; then
            log "VM will not be shutdown."
        elif vagrant_unlocked; then
            log "Shutdown VM after successful build..."
            vagrant suspend | sed 's/^[^,]\+,//' | sed 's/^/[vagrant] /'
        else
            log "VM is busy and will not be shutdown."
        fi
    else
        R=1
    fi
fi

if [ $R -eq 0 ]; then
    ok "Done. Exit code: $R"
else
    error "Done. Exit code: $R"
fi

exit $R
