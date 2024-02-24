#!/bin/sh

set -e

SELF_HOME="$(cd "$(dirname "$0")"; pwd)"
SELF_FILE="$SELF_HOME/$(basename "$0")"
BUILD_HOME="$(pwd)"

if [ "$SELF_HOME" = "$BUILD_HOME" ]; then
    mkdir -p "$SELF_HOME"/build
    cd "$SELF_HOME"/build
    BUILD_HOME="$SELF_HOME"/build
fi

COLORS="33 34 35 36 92 93 94 95 96"

progress() {
    LABEL="$1"
    COLOR="$2"
    while IFS= read -r line; do
        case "$line" in
            \[OK\]*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[32m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            \[ERROR\]*)
                printf '\033[90m[\033[%dm%s\033[90m]\033[0m \033[91m%s\033[0m\n' "$COLOR" "$LABEL" "$line"
                ;;
            make\[*|\[*|mkdir\ *)
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

[ -n "$1" ] || set -- all

if [ "$1" != "build" ] && [ "$1" != "build-local" ]; then

    BUILD_ALL="$BUILD_HOME/all"

    rm -rf "$BUILD_ALL" "$BUILD_HOME/installkit"-*

    [ "$1" != "all" ] && unset BUILD_ALL || set -- Windows MacOS-X Linux-x86 Linux-x86_64

    # Kill all child processes on ctrl-c or exit
    trap "trap - SIGTERM && echo 'Caught SIGTERM, terminating child processes...' && kill -- -$$" SIGINT SIGTERM EXIT

    for PLATFORM; do
        rm -f "$BUILD_HOME/$PLATFORM".*
        rm -rf "$BUILD_HOME/$PLATFORM"
        LOG_OUT="$BUILD_HOME/$PLATFORM.log"
        #{
        #    {
        #        "$SELF_FILE" "$PLATFORM" | tee "$LOG_OUT" | progress out "$PLATFORM"
        #    } 2>&1 1>&3 | tee "$LOG_ERR" | progress err "$PLATFORM"
        #} 3>&1 1>&2 &
        getcolor
        "$SELF_FILE" build "$PLATFORM" 2>&1 | tee "$LOG_OUT" | progress "$PLATFORM" "$CURRENT_COLOR" &
    done

    wait

    trap - SIGINT SIGTERM EXIT

    # Check that build is successful
    for PLATFORM; do
        if [ ! -e "$BUILD_HOME/$PLATFORM.zip" ]; then
            echo "Error: platform '$PLATFORM' failed."
            BUILD_FAILED=1
        fi
    done

    [ -z "$BUILD_FAILED" ] || exit 1

    echo
    if [ -n "$BUILD_ALL" ]; then
        mkdir -p "$BUILD_ALL"
        for PLATFORM; do
            cp -r "$BUILD_HOME/$PLATFORM/kit"/* "$BUILD_ALL"
        done
        VERSION="$(cat "$SELF_HOME"/version)"
        PACKAGE="$BUILD_HOME/installkit-$VERSION.tar.gz"
        echo "Create package: $PACKAGE"
        ( cd "$BUILD_ALL" && tar czf "$PACKAGE" * )
    else
        echo "Build has been launched for specific platforms. Installkit package will not be built."
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

if [ "$1" = "build-local" ] || [ "$2" = "Windows" ]; then
    PLATFORM="$2"
fi

if [ -n "$PLATFORM" ]; then

    log "Start local build..."

    BUILD_DIR="$BUILD_HOME/$PLATFORM"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"/*
    else
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    case "$PLATFORM" in
        Windows)
            # dependencies
            # mingw64-i686-libffi - for ffidl
            for DEP in mingw64-i686-libffi mingw64-i686-gcc-core mingw64-i686-gcc-g++; do
                if [ "$(cygcheck -c -n "$DEP" 2>/dev/null)" = "$DEP" ]; then
                    log "dependency '$DEP' - OK"
                else
                    error "Error: dependency '$DEP' - not installed"
                    error "Please run: setup-x86_64.exe -q -P $DEP"
                    exit 1
                fi
            done
            ARCH_TOOLS_PREFIX="/usr/bin/i686-w64-mingw32-" "$SELF_HOME/configure"
            [ "$MAKE_PARALLEL" != "true" ] || _MAKE_PARALLEL="-j8"
            ;;
        MacOS-X)
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        Linux-x86)
            # dependencies
            # libXcursor-devel - for tkdnd
            sudo yum install -y libgcc.i686 glibc-devel.i686 libX11-devel.i686 libXext-devel.i686 libXt-devel.i686 libXcursor-devel.i686
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        Linux-x86_64)
            # dependencies
            # libXcursor-devel - for tkdnd
            sudo yum install -y libgcc.x86_64 glibc-devel.x86_64 libX11-devel.x86_64 libXext-devel.x86_64 libXt-devel.x86_64 libXcursor-devel.x86_64
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        *)
            "$SELF_HOME/configure"
            ;;
    esac

    if [ "$MAKE_PARALLEL" = "true" ]; then
        [ -n "$_MAKE_PARALLEL" ] && MAKE_PARALLEL="$_MAKE_PARALLEL" || MAKE_PARALLEL="-j2"
        log "Run make with $MAKE_PARALLEL"
    fi

    make $MAKE_PARALLEL
    make $MAKE_PARALLEL check
    make $MAKE_PARALLEL dist

    if [ "$PLATFORM" = "Windows" ] && [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
        ok "Done."
    else
        echo "Done."
    fi

    exit 0

fi

log "Start remote build..."

PLATFORM="$2"

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
        VAGRANT_OPTS="$V_OPTS"
        VAGRANT_KEY="$V_KEY"
        VAGRANT_HOST="$V_USER@$V_HOST"
        VAGRANT_PORT="$V_PORT"
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
MacOS-X      /c/DriveD/VM/loc-70-mac-101206-64
Linux-x86    /c/DriveD/VM/loc-26-lin-cent6-64
Linux-x86_64 /c/DriveD/VM/loc-26-lin-cent6-64
EOF
)"

VAGRANT_VM="$(echo "$VAGRANT_VM" | grep -- "^$PLATFORM " | sed 's/^[^[:space:]]\+[[:space:]]\+//')"

if [ -z "$VAGRANT_VM" ]; then
    log "Error: Unknown platform '$1'" >&2
    exit 1
fi

vagrant_lock hard

while [ "$STATE" != "running" ]; do
    log "Check VM state..."
    STATE="$(vagrant status | grep '^state ' | awk '{print $2}')"
    log "The VM state is: '$STATE'"
    if [ "$STATE" = "poweroff" ] || [ "$STATE" = "saved" ]; then
        log "Start the VM..."
        vagrant up | grep -E '^ui (output|info),' | sed 's/^[^,]\+,//' | sed 's/^/[vagrant] /'
    elif [ "$STATE" != "running" ]; then
        error "Error: unexpected VM state."
        exit 1
    fi
done

log "Get SSH config..."
vagrant_ssh
log "Sync sources..."
logcmd rsync -a --exclude '.git' --exclude 'build' --delete -e "ssh -o StrictHostKeyChecking=no $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT" "$SELF_HOME"/* "$VAGRANT_HOST:installkit-source"

vagrant_lock soft

log "Start build..."
logcmd ssh $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT -o StrictHostKeyChecking=no "$VAGRANT_HOST" "mkdir -p /tmp/work && cd /tmp/work && IK_DEBUG=\"$IK_DEBUG\" MAKE_PARALLEL=\"$MAKE_PARALLEL\" ~/installkit-source/build.sh build-local $PLATFORM" && R=0 || R=$?
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
