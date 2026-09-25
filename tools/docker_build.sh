#!/usr/bin/env bash
# booleos/tools/docker_build.sh
# Builds BooleOS inside the cross-compiler Docker image.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${BOOLEOS_DOCKER_IMAGE:-randomdude/gcc-cross-i686-elf}"
DOCKER_BIN="${DOCKER:-docker}"
UID_OUT="$(id -u)"
GID_OUT="$(id -g)"
DOCKER_ARGS=()

if ! command -v "$DOCKER_BIN" >/dev/null 2>&1; then
    echo "docker_build: docker not found. Install/start Docker first."
    exit 1
fi

if [ "${1:-}" = "clean" ]; then
    DOCKER_ARGS+=(clean)
fi

echo "=== BooleOS Docker build ==="
echo "Image: $IMAGE"
echo "Root:  $ROOT_DIR"
echo ""

"$DOCKER_BIN" run --rm -u root \
    --dns 8.8.8.8 \
    -e BOOLEOS_UID="$UID_OUT" \
    -e BOOLEOS_GID="$GID_OUT" \
    -v "$ROOT_DIR":/booleos:z \
    "$IMAGE" \
    bash -lc '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive

        if ! command -v nasm >/dev/null 2>&1 || ! command -v grub-mkrescue >/dev/null 2>&1 || ! command -v xorriso >/dev/null 2>&1; then
            apt-get update
            apt-get install -y -q nasm grub-pc-bin grub-common xorriso mtools
        fi

        cd /booleos/tools
        if [ "${1:-}" = "clean" ]; then
            make clean
        fi
        make
        chown -R "$BOOLEOS_UID:$BOOLEOS_GID" /booleos/build
    ' bash "${DOCKER_ARGS[@]}"

echo ""
echo "Build complete: $ROOT_DIR/build/booleos.iso"
