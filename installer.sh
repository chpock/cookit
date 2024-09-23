#!/bin/sh

[ -n "$HOME" ] || { echo "\$HOME doesn't defined" >&2; exit 1; }

PREFIX="$HOME/.cookit"
mkdir -p "$PREFIX"
TEMP_FILE="$(mktemp "$PREFIX/installer.XXXXXX")"

cat <<'__COOKIT_ARCHIVE_EOF__' | base64 -d > "$TEMP_FILE"
@ARCHIVE@
__COOKIT_ARCHIVE_EOF__

chmod +x "$TEMP_FILE"
exec "$TEMP_FILE" --install