#!/usr/bin/env python3
"""
tools/make_ramfs.py — generates a flat ramfs image for NullOS.

Image format:
  [uint32_t  num_entries]
  [entry * num_entries]    each entry: name[32] + uint32_t offset + uint32_t size
  [file data]

Usage:
  make_ramfs.py out.img name1=file1.elf [name2=file2.elf ...]
"""

import sys
import struct
import os

ENTRY_NAME_LEN = 32
HEADER_SIZE    = 4                          # uint32_t num_entries
ENTRY_SIZE     = ENTRY_NAME_LEN + 4 + 4    # name[32] + offset + size


def pack_name(name: str) -> bytes:
    encoded = name.encode("ascii")
    if len(encoded) >= ENTRY_NAME_LEN:
        sys.exit(f"error: name '{name}' exceeds {ENTRY_NAME_LEN - 1} characters")
    return encoded + b"\x00" * (ENTRY_NAME_LEN - len(encoded))


def main():
    if len(sys.argv) < 3:
        sys.exit(f"usage: {sys.argv[0]} out.img name=file [name=file ...]")

    out_path = sys.argv[1]
    files = []

    for arg in sys.argv[2:]:
        if "=" not in arg:
            sys.exit(f"error: invalid argument '{arg}' (expected name=file)")
        name, path = arg.split("=", 1)
        if not os.path.isfile(path):
            sys.exit(f"error: file not found: {path}")
        with open(path, "rb") as f:
            data = f.read()
        files.append((name, data))

    num_entries = len(files)
    data_start  = HEADER_SIZE + num_entries * ENTRY_SIZE

    # Calculate offsets for each file
    offset = data_start
    entries = []
    for name, data in files:
        entries.append((name, offset, len(data), data))
        offset += len(data)

    with open(out_path, "wb") as out:
        out.write(struct.pack("<I", num_entries))
        for name, file_offset, size, _ in entries:
            out.write(pack_name(name))
            out.write(struct.pack("<II", file_offset, size))
        for _, _, _, data in entries:
            out.write(data)

    total = data_start + sum(len(d) for _, d in files)
    print(f"ramfs: {num_entries} file(s), {total} bytes → {out_path}")
    for name, file_offset, size, _ in entries:
        print(f"  {name:32s}  offset=0x{file_offset:08x}  size={size}")


if __name__ == "__main__":
    main()
