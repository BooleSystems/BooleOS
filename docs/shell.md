# Interactive shell

- Keyboard ringbuffer (256 chars) in IRQ1; echo removed from the handler
- `SYS_READ (fd=0)`: reads from the ringbuffer with polling + `scheduler_sleep_current(1)` to avoid starving; echoes input and handles backspace
- `user/shell`: loop `> ` → `sys_read` → `run_command`
- Commands: `help`, `uname`, `fetch`, `ps`, `mem`, `ls`, `touch <name>`, `echo <text>`, `kill <pid>`, `run <prog>`, `edit <file>`, `clear`, `exit`
- `fetch`: ASCII banner with OS, Arch, Uptime, free PMM, free Heap, running Procs
- No background debug tasks — VGA is exclusive to the shell

See [testing.md](testing.md) for `run selftest`, the automated regression suite runnable via `run <prog>`.

## Relevant files

```
user/
  shell.c             interactive shell: help/uname/fetch/ps/mem/ls/touch/echo/kill/run/edit/clear/exit
  edit.c              text editor: opens/creates/saves files on FAT16
  selftest.c          automated regression test suite (run via "run selftest")
```
