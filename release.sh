#!/bin/sh

set -e

SELF_HOME="$(cd "$(dirname "$0")"; pwd)"
BUILD_8_DIR="$SELF_HOME/build8"
BUILD_9_DIR="$SELF_HOME/build9"
BUILD_DIR="$SELF_HOME/build"
RELEASE_DIR="$SELF_HOME/release"
INSTALLER="$SELF_HOME/installer.sh"

error() {
    echo "$1" >&2
    exit 1
}

rm -rf "$BUILD_DIR" "$BUILD_8_DIR" "$BUILD_9_DIR" "$RELEASE_DIR"

cd "$SELF_HOME"

./build.sh --tcl-version 8 --buildtype release
mv "$BUILD_DIR" "$BUILD_8_DIR"

./build.sh --tcl-version 9 --buildtype release
mv "$BUILD_DIR" "$BUILD_9_DIR"

VERSION="$(cat "$BUILD_9_DIR"/x86_64-pc-linux-gnu/kit/x86_64-pc-linux-gnu/version)"

mkdir -p "$RELEASE_DIR"

cat <<'EOF' > "$RELEASE_DIR"/windows-installer.tcl
package require cookit::install
::cookit::install::run install {*}$::argv
exit 0
EOF

for PLATFORM in i686-w64-mingw32 x86_64-w64-mingw32 x86_64-apple-darwin10.10 i686-pc-linux-gnu x86_64-pc-linux-gnu; do
    mkdir -p "$RELEASE_DIR/cookit-$VERSION"
    for BUILD_DIR in "$BUILD_8_DIR" "$BUILD_9_DIR"; do
        echo "Copy platform '$PLATFORM' from: $BUILD_DIR ..."
        cp "$BUILD_DIR/$PLATFORM/kit/$PLATFORM/bin"/* "$RELEASE_DIR/cookit-$VERSION"
    done
    echo "Pack platform '$PLATFORM' ..."
    cd "$RELEASE_DIR"
    tar czf "cookit-$VERSION.$PLATFORM.tar.gz" "cookit-$VERSION"
    rm -rf "cookit-$VERSION"
    echo "Create installer for platform '$PLATFORM' ..."
    case "$PLATFORM" in
        *-mingw32)
            "$BUILD_9_DIR"/x86_64-pc-linux-gnu/kit/x86_64-pc-linux-gnu/bin/cookit --wrap \
                "$RELEASE_DIR"/windows-installer.tcl \
                --stubfile "$BUILD_DIR/$PLATFORM/kit/$PLATFORM/bin/cookit-gui.exe" \
                --output "$RELEASE_DIR/cookit-installer.$PLATFORM.exe"
            ;;
        *)
            touch "$RELEASE_DIR/cookit-installer.$PLATFORM.sh"
            while IFS="" read -r line || [ -n "$line" ]; do
                if [ "$line" = "@ARCHIVE@" ]; then
                    base64 < "$BUILD_DIR/$PLATFORM/kit/$PLATFORM/bin/cookit" >> "$RELEASE_DIR/cookit-installer.$PLATFORM.sh"
                else
                    echo "$line" >> "$RELEASE_DIR/cookit-installer.$PLATFORM.sh"
                fi
            done < "$INSTALLER"
            ;;
    esac
done

rm -f "$RELEASE_DIR"/windows-installer.tcl

cd "$SELF_HOME"
echo "Create release archive ..."
tar czf "cookit-release-$VERSION.tar.gz" "build8" "build9" "release"
echo "Cleanup ..."
rm -rf "build8" "build9"

echo "Done."
