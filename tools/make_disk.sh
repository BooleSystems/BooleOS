#!/usr/bin/env bash
# Cria build/disk.img: 32 MB FAT16 com readme.txt na raiz.
# Uso: make_disk.sh <disco.img>
set -e

IMG="$1"
if [ -z "$IMG" ]; then
    echo "uso: make_disk.sh <img>" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
README="$SCRIPT_DIR/readme.txt"

# 32 MB = 65536 setores de 512 bytes
dd if=/dev/zero of="$IMG" bs=512 count=65536 status=none

# Formata como FAT16
mkfs.vfat -F 16 -n NULLOS "$IMG" >/dev/null

# Copia readme.txt para a raiz do disco
if [ -f "$README" ]; then
    mcopy -i "$IMG" "$README" "::/readme.txt"
fi

echo "  disk: 32MB FAT16 → $IMG"
