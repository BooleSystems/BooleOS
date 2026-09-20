# Writing programs for NullOS

This guide is for **writing a NullOS program from the outside**: you do not need to know how the kernel works, only what a program looks like, how to build it, how to get it onto a disk and run it, and what the small user library offers. (For how the system itself is built, see [kernel.md](kernel.md), [syscalls.md](syscalls.md) and the other technical documents.)

## What you need

- The NullOS repository and the `i686-elf-gcc` cross-compiler (see [setup.md](setup.md)), plus QEMU and `mtools` to run and to copy files into the disk image.
- A NullOS ISO built once (`make` in `tools/`): after that you can iterate on a program **without rebuilding the ISO**.

## A program in 10 lines

`sdk/hello.c` is the template:

```c
#include "nullos.h"

void _start(void) {
    printf("Hello from NullOS!\n");
    printf("  my pid is %u\n", nos_getpid());
    nos_exit(0);
}
```

The rules:

- The entry point is **`_start`**. There is no `main()`, no `argc`/`argv` and no C runtime.
- There is **no libc**. Use the small library that ships with the system (below): system-call wrappers (`nos_*`), the usual string/memory functions and a minimal `printf`.
- The program **must end with `nos_exit()`**: `_start` has nowhere to return to.
- No dynamic memory: there is no `malloc()`. Use fixed-size arrays (globals or locals). The stack is small (8 KB), so keep big buffers `static`.
- Programs are 32-bit x86 ELF executables linked at `0x01000000`; the `sdk/Makefile` produces exactly that, so you never deal with the format yourself.

## Build it

```
cd sdk
make                 # builds every *.c in this directory to build/<name>.elf
make hello.elf       # or just one
```

To write your own program, add `myprog.c` next to `hello.c` and run `make` again. The Makefile reuses the same compiler flags, linker script and library sources as the programs shipped with the system, so nothing needs to be copied or configured.

## Run it without rebuilding the ISO

Put the program on the NullOS disk (`build/disk.img` at the repository root) and run it from the shell:

```
cd sdk
make inject PROG=hello          # copies build/hello.elf to the disk as hello.elf
cd ../tools
make run                        # boots NullOS with that disk (the ISO is not rebuilt)
```

then, at the NullOS prompt:

```
> run hello.elf
```

The edit-run loop is: change the `.c` file, `make inject PROG=...`, start QEMU, `run ...`. Notes:

- **Close QEMU before injecting**: do not modify the disk image while QEMU has it open.
- The disk keeps the file until you delete `build/disk.img` (`make clean` in `tools/` does), so you inject once per change, not once per run.
- The kernel only understands **8.3 file names** (up to 8 characters, a dot, up to 3): `hello.elf` is fine, `my_long_program.elf` is not. Names are case-insensitive.
- A program on the disk can be run from any directory with a path (`run docs/hello.elf`), relative to your current directory, like any file. Programs that ship inside the ISO (`shell`, `cat`, `edit`, ...) are looked up first and cannot be replaced by a file with the same name on the disk.
- `run` starts the program and returns to the prompt immediately; the program's output appears on the same screen.
- Programs launched with `run` get no arguments. (A program can start another one with an argument using `nos_exec(name, arg)` and read it with `nos_getarg()`.)
- A program file can be at most 1 MB.

If the shell says `[EXEC] not found: name`, the file is not on the disk (or the name is wrong). `[EXEC] elf_load failed: name` means the file is not a valid NullOS program (for example it was built with the wrong compiler or flags, or it is cut short).

## The library (`sdk` programs include `nullos.h`)

The header `user/lib/nullos.h` is the reference; what each system call does is described in [syscalls.md](syscalls.md). In short:

**Process and time:** `nos_exit(code)`, `nos_getpid()`, `nos_yield()`, `nos_uptime()` (ticks, 100 per second), `nos_fork()`, `nos_wait(pid)`, `nos_kill(pid)`, `nos_ps()`, `nos_meminfo(&pages, &heap, &procs)`.

**Running other programs:** `nos_exec(name, arg)`, `nos_exec_pipe(name, stdin_fd, stdout_fd)`, `nos_pipe(fds)`.

**Input and output:** `nos_write(fd, buf, len)` and `nos_read(fd, buf, len)` (fd 1 is the screen, fd 0 the keyboard), `nos_read_raw()`, `nos_set_raw_mode()`, `nos_kbd_flush()`, `nos_clear()`, `nos_gotoxy(col, row)`, `nos_setcolor(fg, bg)`.

**Files and directories:** `nos_open(name)`, `nos_create(name)`, `nos_close(fd)`, `nos_read`/`nos_write` on file descriptors, `nos_write_file(fd, buf, len)` (replace the whole file), `nos_readdir(path)` (prints a listing), `nos_chdir(path)`, `nos_mkdir(path)`, `nos_getcwd(buf, len)`.

**System:** `nos_getarg(buf, len)`, `nos_pci_list()`, `nos_pci_find(vendor, device)`, `nos_reboot()`, `nos_shutdown()`.

**Strings and memory** (standard names): `memcpy`, `memset`, `memmove`, `memcmp`, `strlen`, `strcmp`, `strncmp`, plus `nos_uitoa(value, buf, size)`.

### `printf` and friends

`printf`, `vprintf`, `sprintf`, `snprintf` and `vsnprintf` have the standard names and signatures, so code written for an ordinary libc usually compiles unchanged. `printf` writes to the screen (fd 1). Supported:

| Conversion | Meaning |
|---|---|
| `%d` `%i` | signed decimal |
| `%u` | unsigned decimal |
| `%x` `%X` | hexadecimal (lower / upper case) |
| `%c` | one character |
| `%s` | string (`(null)` for a null pointer) |
| `%p` | pointer, as `0x` and hex |
| `%%` | a literal `%` |

plus the flags `-` (left-justify), `0` (zero-pad), `+` and space (sign), a field width (digits or `*`), a precision (`.N` or `.*`: minimum digits for numbers, maximum characters for `%s`), and the length modifiers `h`, `hh` and `l`.

**Not supported:** floating point (`%f`, `%e`, `%g`), 64-bit integers (`%lld`), and the `#` flag. An unsupported conversion is printed literally instead of crashing. `snprintf` never writes more than `size` bytes (including the final NUL) and returns the length the full output would have had, as in C99. `sprintf` cannot know the size of your buffer; prefer `snprintf`. The library object is linked separately (the SDK Makefile does it for you), so a program that does not use `printf` does not pay for it.
