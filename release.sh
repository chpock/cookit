#!/bin/sh

set -e

SELF_HOME="$(cd "$(dirname "$0")"; pwd)"
BUILD_8_DIR="$SELF_HOME/build8"
BUILD_9_DIR="$SELF_HOME/build9"
RELEASE_DIR="$SELF_HOME/release"

error() {
    echo "$1" >&2
    exit 1
}

[ -d "$BUILD_8_DIR" ] || error "Build results for Tcl8 don't exist: $BUILD_8_DIR"
[ -d "$BUILD_9_DIR" ] || error "Build results for Tcl9 don't exist: $BUILD_9_DIR"

VERSION="$(cat "$BUILD_9_DIR"/x86_64-pc-linux-gnu/kit/x86_64-pc-linux-gnu/version)"

rm -rf "$RELEASE_DIR"

for PLATFORM in i686-w64-mingw32 x86_64-w64-mingw32 x86_64-apple-darwin10.6 i686-pc-linux-gnu x86_64-pc-linux-gnu; do
    mkdir -p "$RELEASE_DIR/cookit-$VERSION"
    for BUILD_DIR in "$BUILD_8_DIR" "$BUILD_9_DIR"; do
        echo "Copy platform '$PLATFORM' from: $BUILD_DIR ..."
        cp "$BUILD_DIR/$PLATFORM/kit/$PLATFORM/bin"/* "$RELEASE_DIR/cookit-$VERSION"
    done
    echo "Pack platform '$PLATFORM' ..."
    cd "$RELEASE_DIR"
    tar czf "cookit-$VERSION.$PLATFORM.tar.gz" "cookit-$VERSION"
    rm -rf "cookit-$VERSION"
done

cd "$SELF_HOME"
echo "Create release archive ..."
tar czf "cookit-release-$VERSION.tar.gz" "build8" "build9" "release"
echo "Cleanup ..."
rm -rf "build8" "build9"

echo "Done."
