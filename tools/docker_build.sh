#!/usr/bin/env bash
# nullos/tools/docker_build.sh
# Builds NullOS inside the cross-compiler Docker image.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${NULLOS_DOCKER_IMAGE:-randomdude/gcc-cross-i686-elf}"
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

echo "=== NullOS Docker build ==="
echo "Image: $IMAGE"
echo "Root:  $ROOT_DIR"
echo ""

"$DOCKER_BIN" run --rm -u root \
    --dns 8.8.8.8 \
    -e NULLOS_UID="$UID_OUT" \
    -e NULLOS_GID="$GID_OUT" \
    -v "$ROOT_DIR":/nullos:z \
    "$IMAGE" \
    bash -lc '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive

        if ! command -v nasm >/dev/null 2>&1 || ! command -v grub-mkrescue >/dev/null 2>&1 || ! command -v xorriso >/dev/null 2>&1; then
            apt-get update
            apt-get install -y -q nasm grub-pc-bin grub-common xorriso mtools
        fi

        cd /nullos/tools
        if [ "${1:-}" = "clean" ]; then
            make clean
        fi
        make
        chown -R "$NULLOS_UID:$NULLOS_GID" /nullos/build
    ' bash "${DOCKER_ARGS[@]}"

echo ""
echo "Build complete: $ROOT_DIR/build/nullos.iso"
