# Contributing to BooleOS

BooleOS is built entirely with AI-generated code, directed by a human maintainer
rather than hand-typed. That's core to what this project is, not an
implementation detail, so it applies to contributions too.

## The one rule

Pull requests need to be AI-generated, and the commit needs to prove it: a
Co-Authored-By trailer naming the AI tool you used. Claude Code adds this
automatically. If your tool doesn't, add it yourself:

Co-Authored-By: <tool name> <tool-identifier-or-noreply-email>

PRs without this trailer get closed, no exceptions, even if the description
says AI was used. This isn't airtight (nothing is), but every PR still goes
through review, and code that reads as hand-written despite the trailer
gets closed and you'll be asked to redo it properly.

## Before opening a PR

- Read CLAUDE.md, it has the project's hard constraints (32-bit build gate,
  no libc, no real threads, everything through the heap wrapper, SDL2 only
  inside cathode_host). A PR that breaks these gets rejected regardless of
  how it was written.
- Check ROADMAP.md, if what you're building is a planned phase, say which
  one in the PR.
- Build and test locally before opening the PR. `cd tools && make clean &&
  make`, then boot it in QEMU yourself. "AI-generated" doesn't mean
  "untested".

## What to send

- Bug fixes: always welcome.
- Features from the roadmap: welcome, comment on the relevant issue first if
  one exists, to avoid duplicate work.
- New features not on the roadmap: open an issue first to discuss before
  writing code, might not fit the project's direction.
- Docs fixes: welcome, same AI-generation rule applies (typos are the one
  exception, feel free to just fix those by hand).
