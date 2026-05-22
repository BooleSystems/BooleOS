#!/usr/bin/env bash
# nullos/tools/setup_env.sh
# Instala as dependências de build do NullOS no Fedora
# Execute com: bash setup_env.sh

set -e

echo "=== NullOS — Setup do ambiente de build ==="
echo ""

# Detecta distro
if [ -f /etc/fedora-release ]; then
    DISTRO="fedora"
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
else
    echo "AVISO: Distro não reconhecida. Ajuste manualmente."
    DISTRO="unknown"
fi

echo "[1/4] Instalando dependências do sistema..."

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
echo "[2/4] Verificando cross-compiler x86_64-elf-gcc..."
echo ""
echo "  O cross-compiler NAO esta nos repos do Fedora."
echo "  Opcoes:"
echo ""
echo "  A) Compilar do zero (demorado, ~30min):"
echo "     https://wiki.osdev.org/GCC_Cross-Compiler"
echo ""
echo "  B) Usar binarios prontos do osdev-toolchain:"
echo "     https://github.com/lordmilko/i686-elf-tools (para i686)"
echo "     ou compilar via crosstool-ng"
echo ""
echo "  C) Usar uma VM com o toolchain já pronto:"
echo "     docker run -it ghcr.io/osdev/osdev-env"
echo ""

echo "[3/4] Verificando QEMU..."
if command -v qemu-system-x86_64 &>/dev/null; then
    echo "  qemu-system-x86_64: OK ($(qemu-system-x86_64 --version | head -1))"
else
    echo "  QEMU nao encontrado. Instale com: sudo dnf install qemu-system-x86"
fi

echo ""
echo "[4/4] Verificando NASM..."
if command -v nasm &>/dev/null; then
    echo "  nasm: OK ($(nasm --version))"
else
    echo "  NASM nao encontrado. Instale com: sudo dnf install nasm"
fi

echo ""
echo "=== Setup concluido ==="
echo ""
echo "Proximos passos:"
echo "  1. Instale o cross-compiler (opcao acima)"
echo "  2. cd tools && make"
echo "  3. make run"
