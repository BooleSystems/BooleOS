#!/usr/bin/env bash
# nullos/tools/run_qemu.sh
# Runs the built NullOS ISO in QEMU.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="${1:-$ROOT_DIR/build/nullos.iso}"

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "run_qemu: qemu-system-x86_64 not found. Install qemu-system-x86 first."
    exit 1
fi

if [ ! -f "$ISO" ]; then
    echo "run_qemu: ISO not found: $ISO"
    echo "Run: tools/docker_build.sh clean"
    exit 1
fi

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -display sdl
