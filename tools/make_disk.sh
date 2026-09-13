#!/usr/bin/env bash
# Creates build/disk.img: 32 MB FAT16 with readme.txt at the root.
# Usage: make_disk.sh <disk.img>
set -e

IMG="$1"
if [ -z "$IMG" ]; then
    echo "usage: make_disk.sh <img>" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
README="$SCRIPT_DIR/readme.txt"

# 32 MB = 65536 sectors of 512 bytes
dd if=/dev/zero of="$IMG" bs=512 count=65536 status=none

# Format as FAT16
mkfs.vfat -F 16 -n NULLOS "$IMG" >/dev/null

# Copy readme.txt to the root of the disk
if [ -f "$README" ]; then
    mcopy -i "$IMG" "$README" "::/readme.txt"
fi

echo "  disk: 32MB FAT16 → $IMG"
