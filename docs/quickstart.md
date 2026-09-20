# Quick start: run NullOS without building it

If you only want to see the system running, you do not need to compile anything: a release ships a ready-to-boot package. All you need is QEMU (an emulator that runs on Linux, macOS and Windows).

## 1. Get a release

Open the [Releases page](https://github.com/TheShannonDev/NullOS/releases) and download the zip attached to a release (for example `nullos-0.18.0.zip`). Releases without a zip attached only carry the source code.

The zip contains:

| File | What it is |
|---|---|
| `nullos.iso` | the bootable system image (GRUB + kernel + userland) |
| `disk.img` | a blank 32 MB FAT16 disk (only `readme.txt` on it) |
| `README.txt` | a short version of this page |

Extract it anywhere.

## 2. Install QEMU

Get it from the official download page, <https://www.qemu.org/download/>, or from your package manager (for example `sudo apt install qemu-system-x86` or `sudo dnf install qemu-system-x86`).

## 3. Run it

Open a terminal in the extracted folder and run:

```
qemu-system-x86_64 -boot d -cdrom nullos.iso -drive file=disk.img,format=raw,if=ide -m 256M -serial stdio -no-shutdown
```

The command is the same on every platform; only the program name differs slightly: the binary is `qemu-system-x86_64`, or `qemu-system-x86_64.exe` if you call it by its full path on a system that uses `.exe` files.

A window shows the GRUB menu, then the boot log and a shell prompt (`> `). The terminal you launched it from keeps the kernel's serial log.

### The GRUB menu

| Entry | What it does |
|---|---|
| `NullOS v0.18.0` | normal boot (the default, after 3 seconds) |
| `NullOS (serial debug mode)` | the same boot, with the `debug` boot argument |
| `NullOS (Safe Mode)` | the recovery environment: menu, disk info, sector hexdump and a read-only shell (see [safemode.md](safemode.md)) |
| `NullOS v0.17.1 (previous release)` | the previous release's kernel |

The version numbers change from release to release; the last entry is always the release before the one you downloaded.

## 4. Things to try

At the prompt type `help`. Useful commands: `ls`, `cd`, `pwd`, `cat <file>`, `mkdir`, `touch`, `edit <file>` (Ctrl+S saves, Ctrl+Q quits), `mem`, `ps`, `lspci`, `fetch`, `run selftest` (the automated regression suite, see [testing.md](testing.md)), `reboot`, `shutdown`. See [shell.md](shell.md) for the full list.

You do not need the mouse. If QEMU captures your keyboard or mouse, press `Ctrl+Alt+G` to release it.

Safe Mode also starts by itself after 3 boots in a row that never reached the shell prompt.

## Notes

- `disk.img` keeps your files between runs. To start over, extract `disk.img` from the zip again.
- `-serial stdio` shows the kernel's serial log in your terminal; if it gives you trouble on your platform you can drop it.
- Source code and the rest of the documentation: <https://github.com/TheShannonDev/NullOS>. License: MIT.
