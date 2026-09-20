# Documentation TODO

Minimal trail of documentation still owed for work done on the `nightly`
branch. Whenever a relevant code change lands without its full write-up,
leave a one-line stub here instead of leaving no trace, for example:

- `WIP: document <feature> (files: X, Y, Z)`
- `TODO later doc. Related files: ...`

The stubs do not need to be complete at every commit. They are resolved
during the final polish of each version, before `nightly` is merged into
`main`: write the real description into the relevant `docs/<subject>.md`,
then delete the stub from this file. This file should be empty (headers
only) whenever a version is closed.

## Pending

Phase 19 (SDK / app-development experience), work in progress:

- WIP: documentar o selftest de 22 testes e o `make test-elf` em `docs/testing.md`
  (arquivos: user/selftest.c, tools/test_elf_load.c, tools/Makefile) — novos
  testes: exec de programa só no FAT16, rejeição de lixo/ELF truncado/inexistente,
  família printf; deixam `st_cat.elf`, `st_bad.bin`, `st_trunc.elf` no disco.
- WIP: documentar `exec()` como novo consumidor de `vfs_open()` e o hazard de
  `sector_buf`/`dir_buf` compartilhados (Fase 28) que agora também vale para as
  leituras bloqueantes do `exec()` em `docs/filesystem.md`
  (arquivos: kernel/exec.c, kernel/fs/vfs.c, kernel/fs/fat16.c).
- WIP: documentar a linha `SYS_EXEC` de `docs/syscalls.md`: o nome agora é
  resolvido por `vfs_open()` (ramfs primeiro, depois FAT16 contra o cwd), limite
  de 192 KB (arquivos: kernel/syscall.c, kernel/exec.c).
- WIP: documentar o endurecimento do carregador ELF (tamanho do arquivo,
  faixa `[0x800000, 0x02000000)`, aritmética de 64 bits, validação antes de
  mapear) em `docs/security.md` (arquivos: kernel/elf.c, kernel/elf.h).
- WIP: documentar que `run` aceita um caminho de programa no FAT16 em
  `docs/shell.md` (arquivos: user/shell.c, kernel/exec.c).
- WIP: documentar `make test-elf` e a dependência order-only do `user` (`make
  run` funciona logo depois de `make clean`) em `docs/setup.md`
  (arquivos: tools/Makefile, tools/test_elf_load.c).
- WIP: README: lista de arquivos (`sdk/`, `user/lib/nosstdio.c`,
  `tools/test_elf_load.c`), link de `docs/sdk.md` na lista de docs, `make
  test-elf` na seção Build (arquivos: README.md).
- At the phase close: README (completed-phases table + banner), CHANGELOG (the
  `[Unreleased]` entry renamed to one `[0.19.0]`), ROADMAP (Phase 19 done; the
  Phase 28 item "elf_load never receives the file size / page_end overflow" is
  now fixed), `version.h`, PROGRESS. Ver "Definition of Done" no CLAUDE.md.
