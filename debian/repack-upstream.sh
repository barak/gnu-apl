#!/bin/sh
set -e

PACKAGE_NAME=$(dpkg-parsechangelog -S Source)
VERSION=""
TARBALL=""

while [ $# -gt 0 ]; do
    case "$1" in
        --upstream-version)
            VERSION="$2"
            shift 2
            ;;
        *)
            TARBALL="$1"
            shift
            ;;
    esac
done

if [ -z "$VERSION" ]; then
    echo "Error: Missing version argument from uscan." >&2
    exit 1
fi

# Fallback layout: If uscan omitted the filename because mk-origtargz already ran,
# reconstruct the standard Debian naming convention path.
if [ -z "$TARBALL" ]; then
    TARBALL="../${PACKAGE_NAME}_${VERSION}.orig.tar.xz"
fi

if [ ! -f "$TARBALL" ]; then
    echo "Error: Target tarball '$TARBALL' does not exist." >&2
    exit 1
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM HUP

echo "Inspecting and restructuring upstream tarball: $TARBALL"

# Extract the archive into our sandbox
tar -C "$TMP_DIR" -xf "$TARBALL"

# Infer the top-level directory layout dynamically
UPSTREAM_DIR=$(tar -tf "$TARBALL" | head -n 1 | cut -d/ -f1)
TARGET_DIR="${PACKAGE_NAME}-${VERSION}"

if [ -d "$TMP_DIR/$UPSTREAM_DIR" ]; then
    # If the directory matches what we want, nothing needs changing
    if [ "$UPSTREAM_DIR" = "$TARGET_DIR" ]; then
        echo "Tarball layout inside '$TARBALL' is already correct."
        exit 0
    fi

    echo "Renaming root directory '$UPSTREAM_DIR' to '$TARGET_DIR'..."
    mv "$TMP_DIR/$UPSTREAM_DIR" "$TMP_DIR/$TARGET_DIR"

    # Compress directly into the isolated sandbox under a temp name
    tar -C "$TMP_DIR" -cJf "$TMP_DIR/repacked.tar.xz" "$TARGET_DIR"

    # Safely remove the un-restructured archive
    rm -f "$TARBALL"

    # Move the finished product into place atomically
    mv "$TMP_DIR/repacked.tar.xz" "$TARBALL"

    echo "Successfully updated internal directory structure inside $TARBALL"
else
    echo "Error: Expected directory '$UPSTREAM_DIR' not found during extraction." >&2
    exit 1
fi
