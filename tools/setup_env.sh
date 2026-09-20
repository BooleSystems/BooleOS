#!/usr/bin/env bash
# nullos/tools/setup_env.sh
# Installs NullOS's build dependencies on Fedora
# Run with: bash setup_env.sh

set -e

echo "=== NullOS — Build environment setup ==="
echo ""

# Detect distro
if [ -f /etc/fedora-release ]; then
    DISTRO="fedora"
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
else
    echo "WARNING: Unrecognized distro. Adjust manually."
    DISTRO="unknown"
fi

echo "[1/4] Installing system dependencies..."

if [ "$DISTRO" = "fedora" ]; then
    sudo dnf install -y \
        nasm \
        grub2-tools \
        xorriso \
        qemu-system-x86 \
        gdb \
        make \
        gcc

elif [ "$DISTRO" = "debian" ]; then
    sudo apt-get install -y \
        nasm \
        grub-common \
        grub-pc-bin \
        xorriso \
        qemu-system-x86 \
        gdb \
        make \
        gcc
fi

echo ""
echo "[2/4] Checking for the x86_64-elf-gcc cross-compiler..."
echo ""
echo "  The cross-compiler is NOT in the Fedora repos."
echo "  Options:"
echo ""
echo "  A) Build it from scratch (slow, ~30min):"
echo "     https://wiki.osdev.org/GCC_Cross-Compiler"
echo ""
echo "  B) Use prebuilt osdev-toolchain binaries:"
echo "     https://github.com/lordmilko/i686-elf-tools (for i686)"
echo "     or build via crosstool-ng"
echo ""
echo "  C) Use the automatic Docker build (recommended):"
echo "     tools/docker_build.sh clean"
echo ""

echo "[3/4] Checking QEMU..."
if command -v qemu-system-x86_64 &>/dev/null; then
    echo "  qemu-system-x86_64: OK ($(qemu-system-x86_64 --version | head -1))"
else
    echo "  QEMU not found. Install with: sudo dnf install qemu-system-x86"
fi

echo ""
echo "[4/4] Checking NASM..."
if command -v nasm &>/dev/null; then
    echo "  nasm: OK ($(nasm --version))"
else
    echo "  NASM not found. Install with: sudo dnf install nasm"
fi

echo ""
echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "  1. tools/docker_build.sh clean"
echo "  2. cd tools && make run"
