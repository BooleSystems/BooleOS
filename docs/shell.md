# Interactive shell

- Keyboard ringbuffer (256 chars) in IRQ1; echo removed from the handler
- `SYS_READ (fd=0)`: reads from the ringbuffer with polling + `scheduler_sleep_current(1)` to avoid starving; echoes input and handles backspace
- `user/shell`: loop `> ` → `sys_read` → `run_command`
- Commands: `help`, `uname`, `fetch`, `ps`, `mem`, `ls [dir]`, `lspci`, `touch <name>`, `mkdir <dir>`, `cd [dir]`, `echo <text>`, `kill <pid>`, `run <prog>`, `edit <file>`, `cmd1 | cmd2`, `pwd`, `cat [file]`, `reboot`, `shutdown`, `cmd < file`, `cmd > file`, `clear`, `exit`. `touch`/`edit`/`ls`/`mkdir`/`cd` accept a path with a subfolder (e.g. `edit docs/notes.txt`) — see `docs/filesystem.md` → "Subdirectories"
- `cmd1 | cmd2` (Phase 16): pipes `cmd1`'s stdout into `cmd2`'s stdin via `SYS_PIPE`/`SYS_EXEC_PIPE` — both sides must be real programs (ramfs ELFs), not shell builtins, since builtins like `ps`/`echo` write straight to VGA and never touch fd 1. See `docs/pipes.md` for the full design and `docs/testing.md` for a working example (`forktest | cat`).
- `pwd` (`SYS_GETCWD`), `cat <file>` (launches `user/cat.c`, which prints the file; with no argument `cat` copies stdin), `reboot` / `shutdown` (`SYS_REBOOT`/`SYS_SHUTDOWN`, see `docs/kernel.md` → "Power")
- `cmd < file` / `cmd > file` (`run_redirected()`): the file is opened (`>` creates and truncates) and passed as an fd through the same `SYS_EXEC_PIPE` the pipe uses — the kernel copies whatever fd it is given, pipe end or file. External programs only, launched by name without arguments, and not combinable with `|`. Builtins can't be redirected (they print from the shell process or the kernel, and a process can't redirect its own fd 1 without a dup2-style syscall)
- The shell strips the trailing `\n`/`\r` from each line right after reading it, so a bare command name (`edit`, `cat`, `run`) matches its branch; `run` has one implementation (`cmd_run()`) shared by the foreground path and `run_command()`
- `run <prog>` (Phase 19): the program is found by `exec()` like any file — the ramfs first (`run shell`, `run selftest`), then FAT16 relative to the shell's current directory, so a program you put on the disk with `make inject` (see `docs/sdk.md`) runs with `run hello.elf`, and `run dir/hello.elf` works from a parent directory. 8.3 names, case-insensitive. A missing file gives `[EXEC] not found`, a file that is not a valid program `[EXEC] elf_load failed`. The program starts in the background and the prompt returns immediately; `run` passes it no argument.
- `fetch`: ASCII banner with OS, Arch, Uptime, free PMM, free Heap, running Procs
- No background debug tasks — VGA is exclusive to the shell

See [testing.md](testing.md) for `run selftest`, the automated regression suite runnable via `run <prog>`.

## Relevant files

```
user/
  shell.c             interactive shell: help/uname/fetch/ps/mem/ls/touch/mkdir/cd/pwd/echo/kill/run/edit/cat/cmd1|cmd2/redirects/reboot/shutdown/clear/exit
  edit.c              text editor: opens/creates/saves files on FAT16
  selftest.c          automated regression test suite (run via "run selftest")
  cat.c               prints a file (with an argument) or copies stdin to stdout (pipe sink) — see docs/pipes.md
```
