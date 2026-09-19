# Syscalls

- `jump_to_usermode` via `iret` with ring 3 segments (CS=0x1B, SS=0x23)
- Isolation via per-process CR3
- Syscall gate: `int 0x80`, convention `eax=num, ebx=arg1, ecx=arg2, edx=arg3`

For how every userland-supplied pointer/string argument below is validated
before the kernel touches it, see [security.md](security.md).

| num | name | signature |
|-----|------|------------|
| 1 | `SYS_WRITE` | `write(fd, buf, len) → bytes` |
| 2 | `SYS_EXIT` | `exit(code) → does not return` |
| 3 | `SYS_YIELD` | `yield() → 0` |
| 4 | `SYS_GETPID` | `getpid() → pid` |
| 5 | `SYS_READ` | `read(fd, buf, len) → bytes read` |
| 6 | `SYS_UPTIME` | `uptime() → ticks (100 Hz)` |
| 7 | `SYS_MEMINFO` | `meminfo(*pmm_pages, *heap_bytes, *nprocs) → 0` |
| 8 | `SYS_PS` | `ps() → 0` (prints the process table via VGA) |
| 9 | `SYS_KILL` | `kill(pid) → 0 or -1` |
| 10 | `SYS_EXEC` | `exec(name) → pid or -1` |
| 11 | `SYS_OPEN` | `open(name) → fd (≥3) or -1` |
| 12 | `SYS_CLOSE` | `close(fd) → 0 or -1` |
| 13 | `SYS_READ_RAW` | `read_raw() → scancode\|(ctrl<<8)` (blocking, no echo) |
| 14 | `SYS_GOTOXY` | `gotoxy(col, row) → 0` |
| 15 | `SYS_CLEAR` | `clear() → 0` |
| 16 | `SYS_GETARG` | `getarg(buf, len) → bytes or -1` (argument passed by `SYS_EXEC`) |
| 17 | `SYS_KBD_FLUSH` | `kbd_flush() → 0` (flushes keyboard buffers) |
| 18 | `SYS_SETCOLOR` | `set_color(fg, bg) → 0` |
| 19 | `SYS_SET_RAW_MODE` | `set_raw_mode(1/0) → 0` (turns off `SYS_READ` echo) |
| 20 | `SYS_WAIT` | `wait(pid) → 0` (blocks until the process terminates) |
| 21 | `SYS_READDIR` | `readdir(path) → 0 or -1` (lists ramfs + FAT16 via VGA; `path` may be NULL/empty for the caller's cwd — see `SYS_CHDIR`) |
| 22 | `SYS_WRITE_FILE` | `write_file(fd, buf, len) → 0 or -1` (writes to FAT16) |
| 23 | `SYS_CREATE` | `create(name) → fd (≥3) or -1` (opens if it exists, otherwise creates it empty on FAT16) |
| 24 | `SYS_PCI_LIST` | `pci_list() → device count` (reprints the PCI device table found at boot via VGA, without rescanning; also returns `pci_device_count()` so a caller can check "found anything?" without parsing VGA text — added for `user/selftest.c`) |
| 25 | `SYS_FORK` | `fork() → child's pid (parent) / 0 (child) / -1` (duplicates the caller: full address space, open fds; not copy-on-write) |
| 26 | `SYS_CHDIR` | `chdir(path) → 0 or -1` (changes the caller's FAT16 cwd; only mutates it on confirmed success — a missing path, a path naming a file, or an I/O error all leave the cwd untouched) |
| 27 | `SYS_MKDIR` | `mkdir(path) → 0 or -1` (creates a directory on FAT16; idempotent if a directory of that name already exists, fails if a file does) |

> `SYS_READ` is polymorphic: fd=0 reads from the keyboard (blocking, with echo and backspace); fd≥3 reads from a file opened via `SYS_OPEN`/`SYS_CREATE`, advances the position, and returns 0 on EOF.

> `SYS_WRITE` (fd=1/2) writes to VGA; files use the dedicated `SYS_WRITE_FILE`, which writes to FAT16 via `vfs_write`/`fat16_write_file` — ramfs remains read-only.

> **Syscall return value:** `isr128` writes the `syscall_handler`'s return value into the EAX slot of the `pusha` frame before the `popa`, delivering the correct value in `eax` to userland after the `iret`. Userland inline asm must use the `"=a"`/`"0"` constraints so the compiler doesn't assume eax is unchanged after `int $0x80`.

## Relevant files

```
kernel/
  isr.asm             Exception stubs and the syscall gate (isr128)
  syscall.c/h         Syscall dispatcher
```

> Whenever this table is updated, `kernel/syscall.h` (the `#define SYS_*` values) is the single source of truth — confirm every number here against it (see CLAUDE.md, "Convenções de fim de fase (números de syscall)").
