# Quick start: run BooleOS without building it

If you only want to see the system running, you do not need to compile anything: a release ships a ready-to-boot package. All you need is QEMU (an emulator that runs on Linux, macOS and Windows).

## 1. Get a release

Open the [Releases page](https://github.com/TheShannonDev/NullOS/releases) and download the zip attached to a release (named like `booleos-X.Y.Z.zip`). Releases without a zip attached only carry the source code.

The zip contains:

| File | What it is |
|---|---|
| `booleos.iso` | the bootable system image (GRUB + kernel + userland) |
| `disk.img` | a blank 32 MB FAT16 disk (only `readme.txt` on it) |
| `README.txt` | a short version of this page |

Extract it anywhere.

## 2. Install QEMU

QEMU is free and runs on Linux, macOS and Windows. The official download page is <https://www.qemu.org/download/>; it lists the options for each platform. You need the x86-64 system emulator, the program called `qemu-system-x86_64`.

**Linux** - use your package manager:

```
sudo apt install qemu-system-x86        # Debian, Ubuntu
sudo dnf install qemu-system-x86        # Fedora
sudo pacman -S qemu-system-x86          # Arch
```

**macOS** - install it with [Homebrew](https://brew.sh/):

```
brew install qemu
```

(MacPorts also has a package: `sudo port install qemu`. No compiling is involved either way; these install prebuilt binaries.) On Apple Silicon Macs QEMU emulates the x86 processor in software, so BooleOS boots a bit slower than on an Intel machine, but it works.

**Windows** - download the installer from the official page above (it links the Windows builds, which are also available at <https://qemu.weilnetz.de/w64/>) and run it. By default it installs to `C:\Program Files\qemu`, which is not added to your `PATH`: either call the program by its full path (see below) or add that folder to `PATH`.

Check that it works by running `qemu-system-x86_64 --version` (on Windows, in the folder above, or with the full path).

## 3. Run it

Open a terminal in the extracted folder and run:

```
qemu-system-x86_64 -boot d -cdrom booleos.iso -drive file=disk.img,format=raw,if=ide -m 256M -serial stdio -no-shutdown
```

The arguments are the same on every platform. What changes is how you call the program:

- **Linux, macOS:** `qemu-system-x86_64 ...` as above.
- **Windows** (Command Prompt): if QEMU is not on your `PATH`, use the full path in quotes, for example `"C:\Program Files\qemu\qemu-system-x86_64.exe" -boot d -cdrom booleos.iso ...` (in PowerShell put `&` in front of the quoted path).

A window shows the GRUB menu, then the boot log and a shell prompt (`> `). The terminal you launched it from keeps the kernel's serial log.

### The GRUB menu

| Entry | What it does |
|---|---|
| `BooleOS vX.Y.Z` | normal boot (the default, after 3 seconds) |
| `BooleOS (serial debug mode)` | the same boot, with the `debug` boot argument: extra `[DEBUG]` detail on the serial port (the terminal you launched QEMU from) |
| `BooleOS (Safe Mode)` | the recovery environment: menu, disk info, sector hexdump and a read-only shell (see [safemode.md](safemode.md)) |
| `BooleOS vA.B.C (previous release)` | the previous release's kernel |

`vX.Y.Z` stands for the version you downloaded and `vA.B.C` for the release before it; the real numbers are shown in the menu.

## 4. Things to try

At the prompt type `help`. Useful commands: `ls`, `cd`, `pwd`, `cat <file>`, `mkdir`, `touch`, `edit <file>` (Ctrl+S saves, Ctrl+Q quits), `mem`, `ps`, `lspci`, `fetch`, `run selftest` (the automated regression suite, see [testing.md](testing.md)), `reboot`, `shutdown`. See [shell.md](shell.md) for the full list.

You do not need the mouse. If QEMU captures your keyboard or mouse, press `Ctrl+Alt+G` to release it.

Safe Mode also starts by itself after 3 boots in a row that never reached the shell prompt.

## Notes

- `disk.img` keeps your files between runs. To start over, extract `disk.img` from the zip again.
- `-serial stdio` shows the kernel's serial log in your terminal; if it gives you trouble on your platform you can drop it.
- Source code and the rest of the documentation: <https://github.com/TheShannonDev/NullOS>. License: MIT.

## Running it in other virtual machine programs

QEMU is the tested way to run BooleOS, and the project's own testing has only been done with it. Other hypervisors should work if they provide the same kind of virtual PC, but the steps below are general guidance based on each product's standard settings, not something the project has verified. If you try one, reports of what worked are welcome (open an issue on the repository).

### What any hypervisor needs

- **The ISO as a boot CD/DVD:** attach `booleos.iso` as an optical drive and make the VM boot from it (the ISO contains GRUB, the kernel and the userland).
- **`disk.img` as a hard disk:** attach it as an additional **IDE** disk (SATA is not supported by BooleOS's disk driver, only legacy IDE/ATA). BooleOS uses the first hard disk it finds; the optical drive does not get in the way.
- **RAM:** about 256 MB is plenty (BooleOS itself uses only a few MB).
- **A plain PC:** classic **BIOS** firmware (not UEFI), a PS/2 keyboard and standard VGA text mode. BooleOS has no network, USB, sound or graphics-mode driver, so leave those at their defaults or disable them.
- **`disk.img` is a raw disk image.** VirtualBox and VMware do not accept raw images directly, so convert it once. `qemu-img` comes with the QEMU you installed:

  ```
  qemu-img convert -f raw -O vdi  disk.img disk.vdi       # for VirtualBox
  qemu-img convert -f raw -O vmdk -o subformat=monolithicSparse disk.img disk.vmdk   # for VMware
  ```

  (VirtualBox users can also run `VBoxManage convertfromraw disk.img disk.vdi --format VDI`.) The conversion keeps the contents; the converted file behaves as the blank 32 MB disk.

### VirtualBox

1. **Machine > New** (the *New* button). Give it a name (for example "BooleOS"), leave the ISO Image field empty, set **Type: Other** and **Version: Other/Unknown**. Set the base memory to **256 MB** and choose **Do not add a virtual hard disk**. Finish the wizard.
2. Select the VM and open **Settings > Storage**.
3. Under the **IDE** controller (add one with the controller icon if it is not there), click the **Adds optical drive** icon, choose **Choose a disk file...** and pick `booleos.iso`.
4. Still under the IDE controller, click the **Adds hard disk** icon, choose **Add** (or *Choose existing disk*) and pick the `disk.vdi` you converted.
5. Check **Settings > System > Motherboard**: the boot order should list *Optical* before *Hard Disk*, and **Enable EFI** should be off.
6. Press **Start**. The GRUB menu appears; press Enter for the default entry.

### VMware Workstation / Workstation Player

1. **File > New Virtual Machine...** and pick **Custom (advanced)**. Keep the hardware compatibility default.
2. On the guest operating system installation page choose **I will install the operating system later**.
3. Choose **Guest operating system: Other**, version **Other** (a 32-bit option if one is offered).
4. Name it, and on the firmware page choose **BIOS** (not UEFI). Set the memory to **256 MB**. Leave the processor and network choices at their defaults.
5. When asked for the disk type choose **IDE**. On the disk page choose **Use an existing virtual disk** and select the `disk.vmdk` you converted (keep the existing format if asked).
6. Finish, then open **VM > Settings** (Workstation Player: **Player > Manage > Virtual Machine Settings**) and select **CD/DVD (IDE)** in the **Hardware** list. Choose **Use ISO image file**, browse to `booleos.iso` and tick **Connect at power on**.
7. Power the VM on. The GRUB menu appears; press Enter for the default entry.

### If something does not work

- If the screen stays blank or the machine resets right away, check that the VM is set to **BIOS** firmware and **IDE** for the disk.
- The shell's `shutdown` command uses power-management hardware that QEMU emulates; other hypervisors may not honor it (the kernel prints a message if so). Close the VM window instead.
