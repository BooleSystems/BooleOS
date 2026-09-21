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

Phase 20 (crash handler leads into Safe Mode), work in progress:

- WIP: documentar o teste manual do crash handler em `docs/testing.md` (o
  procedimento com `make run-reboot-test` e `crash <de|pf|gpf>` está em
  `docs/safemode.md`; falta o ponteiro/resumo em testing.md)
  (arquivos: kernel/crashdump.c, kernel/idt.c, user/shell.c).
- TODO later doc. Related files: docs/setup.md — `make run-reboot-test` é o
  alvo certo para testar o crash handler (o `run` tem `-no-reboot`)
  (arquivos: tools/Makefile).
- TODO later: teste de kernel-mode fault (hoje o `crash` falha em ring 3), da
  guarda de re-entrada (falha durante a gravação) e do caminho "não salvou"
  (crash antes do disco) — não cobertos pelo teste manual atual
  (arquivos: kernel/idt.c, kernel/crashdump.c).
- At the phase close: README (table + banner), CHANGELOG (`[Unreleased]` ->
  `[0.20.0]`), ROADMAP (renumber: this phase becomes 20, the old 20-30 shift by
  one; mark it done), `version.h`, PROGRESS. Ver "Definition of Done" no
  CLAUDE.md.
