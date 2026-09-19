# Interactive shell

- Keyboard ringbuffer (256 chars) in IRQ1; echo removed from the handler
- `SYS_READ (fd=0)`: reads from the ringbuffer with polling + `scheduler_sleep_current(1)` to avoid starving; echoes input and handles backspace
- `user/shell`: loop `> ` → `sys_read` → `run_command`
- Commands: `help`, `uname`, `fetch`, `ps`, `mem`, `ls [dir]`, `lspci`, `touch <name>`, `mkdir <dir>`, `cd [dir]`, `echo <text>`, `kill <pid>`, `run <prog>`, `edit <file>`, `cmd1 | cmd2`, `clear`, `exit`. `touch`/`edit`/`ls`/`mkdir`/`cd` accept a path with a subfolder (e.g. `edit docs/notes.txt`) — see `docs/filesystem.md` → "Subdirectories"
- `cmd1 | cmd2` (Phase 16): pipes `cmd1`'s stdout into `cmd2`'s stdin via `SYS_PIPE`/`SYS_EXEC_PIPE` — both sides must be real programs (ramfs ELFs), not shell builtins, since builtins like `ps`/`echo` write straight to VGA and never touch fd 1. See `docs/pipes.md` for the full design and `docs/testing.md` for a working example (`forktest | cat`).
- `fetch`: ASCII banner with OS, Arch, Uptime, free PMM, free Heap, running Procs
- No background debug tasks — VGA is exclusive to the shell

See [testing.md](testing.md) for `run selftest`, the automated regression suite runnable via `run <prog>`.

## Relevant files

```
user/
  shell.c             interactive shell: help/uname/fetch/ps/mem/ls/touch/mkdir/cd/echo/kill/run/edit/cmd1|cmd2/clear/exit
  edit.c              text editor: opens/creates/saves files on FAT16
  selftest.c          automated regression test suite (run via "run selftest")
  cat.c               minimal pipe sink (reads stdin, writes stdout) — see docs/pipes.md
```
