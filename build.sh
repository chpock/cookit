#!/bin/sh

set -e

SELF_HOME="$(cd "$(dirname "$0")"; pwd)"

BUILD_HOME="$(pwd)"

COLORS="32 33 34 35 36 92 93 94 95 96"

progress() {
    LABEL="$1"
    COLOR="$2"
    while IFS= read -r line; do
        case "$line" in
            make[*|[*|mkdir\ *)
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

    [ "$1" != "all" ] || set -- Windows MacOS-X Linux-x86 Linux-x86_64

    for PLATFORM; do
        rm -f "$BUILD_HOME/$PLATFORM".*
        LOG_OUT="$BUILD_HOME/$PLATFORM.log"
        #{
        #    {
        #        "$0" "$PLATFORM" | tee "$LOG_OUT" | progress out "$PLATFORM"
        #    } 2>&1 1>&3 | tee "$LOG_ERR" | progress err "$PLATFORM"
        #} 3>&1 1>&2 &
        getcolor
        "$0" build "$PLATFORM" 2>&1 | tee "$LOG_OUT" | progress "$PLATFORM" "$CURRENT_COLOR" &
    done

    wait

    exit 0

fi

log() {
    echo "[INFO] $1"
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

    BUILD_DIR="$BUILD_HOME/build-$PLATFORM"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"/*
    else
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    case "$PLATFORM" in
        Windows)
            # dependencies
            for DEP in mingw64-i686-libffi mingw64-i686-gcc-core mingw64-i686-gcc-g++; do
                if [ "$(cygcheck -c -n "$DEP" 2>/dev/null)" = "$DEP" ]; then
                    log "dependency '$DEP' - OK"
                else
                    log "Error: dependency '$DEP' - not installed"
                    log "Please run: setup-x86_64.exe -q -P $DEP"
                    exit 1
                fi
            done
            ARCH_TOOLS_PREFIX="/usr/bin/i686-w64-mingw32-" "$SELF_HOME/configure"
            ;;
        MacOS-X)
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        Linux-x86)
            # dependencies
            sudo yum install -y libgcc.i686 glibc-devel.i686 libX11-devel.i686 libXext-devel.i686 libXt-devel.i686
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        Linux-x86_64)
            # dependencies
            sudo yum install -y libgcc.x86_64 glibc-devel.x86_64 libX11-devel.x86_64 libXext-devel.x86_64 libXt-devel.x86_64
            PLATFORM="$PLATFORM" "$SELF_HOME/configure"
            ;;
        *)
            "$SELF_HOME/configure"
            ;;
    esac

    make
    make check CHECK_SAFE=1
    make dist

    if [ "$PLATFORM" = "Windows" ] && [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
    fi

    log "Done."
    exit 0

fi

log "Start remote build..."

PLATFORM="$2"

BUILD_DIR="$BUILD_HOME/build-$PLATFORM"
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"/*
else
    mkdir -p "$BUILD_DIR"
fi

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
            log "Error: Timeout reached."
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
    if [ "$STATE" = "poweroff" ]; then
        log "Start the VM..."
        vagrant up | grep -E '^ui (output|info),' | sed 's/^[^,]\+,//' | sed 's/^/[vagrant] /'
    elif [ "$STATE" != "running" ]; then
        log "Error: unexpected VM state."
        exit 1
    fi
done

vagrant_lock soft

log "Get SSH config..."
vagrant_ssh
log "Sync sources..."
logcmd rsync -a --exclude '.git' -e "ssh -o StrictHostKeyChecking=no $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT" "$SELF_HOME"/* "$VAGRANT_HOST:installkit-source"
log "Start build..."
logcmd ssh $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT -o StrictHostKeyChecking=no "$VAGRANT_HOST" "mkdir -p /tmp/work && cd /tmp/work && ~/installkit-source/build.sh build-local $PLATFORM" && R=0 || R=$?
log "Sync build results..."
logcmd rsync -a -e "ssh -o StrictHostKeyChecking=no $VAGRANT_OPTS $VAGRANT_KEY -p $VAGRANT_PORT" "$VAGRANT_HOST:/tmp/work/build-$PLATFORM/*" "$BUILD_DIR"

vagrant_unlock

if [ "$R" -eq 0 ]; then
    if [ -e "$BUILD_DIR/$PLATFORM.zip" ]; then
        mv "$BUILD_DIR/$PLATFORM.zip" "$BUILD_DIR/.."
        if vagrant_unlocked; then
            log "Shutdown VM after successful build..."
            vagrant halt | sed 's/^[^,]\+,//' | sed 's/^/[vagrant] /'
        else
            log "VM is busy and will not be shutdown."
        fi
    else
        R=1
    fi
fi

log "Done. Exit code: $R"
exit $R
