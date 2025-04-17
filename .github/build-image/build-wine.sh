#!/bin/bash

# Default values
OUTPUT="./build"
JOBS=""

# Parse arguments
for arg in "$@"; do
    case $arg in
        -src=*)
            SRC="${arg#*=}"
            ;;
        -output=*)
            OUTPUT="${arg#*=}"
            ;;
        -j=*)
            JOBS="-j${arg#*=}"
            ;;
    esac
done



# Ensure source path is set
if [ -z "$SRC" ]; then
    echo "Error: Source path must be specified with -src=path/to/source"
    exit 1
fi
# Get the absolute path of where the script was executed
BASE_DIR="$(pwd)"

# Convert SRC to absolute path if it's not already absolute
case "$SRC" in
    /*) ;;  # Already absolute, do nothing
    *) SRC="$BASE_DIR/$SRC" ;;  # Convert to absolute
esac

# Convert OUTPUT to absolute path if it's not already absolute
case "$OUTPUT" in
    /*) ;;  # Already absolute, do nothing
    *) OUTPUT="$BASE_DIR/$OUTPUT" ;;  # Convert to absolute
esac

if [ -d "$OUTPUT" ]; then
    rm -rf "$OUTPUT"
fi



# Create build directories
mkdir -p "$OUTPUT/x32" "$OUTPUT/x64"

# Build x64
cd "$OUTPUT/x64" || exit 1
$SRC/configure CC="ccache gcc" x86_64_CC="ccache x86_64-w64-mingw32-gcc" \
    CFLAGS="-march=native -O3 -pipe -fstack-protector-strong" --enable-win64
make $JOBS

# Build x32
cd "$OUTPUT/x32" || exit 1
PKG_CONFIG_PATH=/usr/lib/pkgconfig $SRC/configure CC="ccache gcc" \
    i386_CC="ccache i686-w64-mingw32-gcc" CFLAGS="-march=native -O3 -pipe -fstack-protector-strong" \
    --with-wine64=../x64
make $JOBS

mkdir -p "$OUTPUT/package" 

# install
cd "$OUTPUT/x64" || exit 1
make install DESTDIR="$OUTPUT/package"

cd "$OUTPUT/x32" || exit 1
make install DESTDIR="$OUTPUT/package"

# create config
cd "$OUTPUT/package" || exit 1
mkdir -p ./DEBIAN
cp /control ./DEBIAN/control
dpkg-deb --build $OUTPUT/package
ls
ls /build