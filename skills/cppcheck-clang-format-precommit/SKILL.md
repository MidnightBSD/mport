---
name: cppcheck-clang-format-precommit
description: Run cppcheck and clang-format (prefer clang-format19) on staged C/C header changes before committing; block commits on cppcheck errors/warnings and auto-format code.
---

# C pre-commit sanity (cppcheck + clang-format)

Use this skill when you’re about to `git commit` C/C header changes and want an automated preflight for serious bugs/vulns and formatting consistency.

## Workflow

1. Stage your changes (`git add ...`).
2. Run the pre-commit script:
   - `./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh`
3. If it exits non-zero, fix findings and re-run until clean.
4. Commit.

## What it does

- Formats staged `*.c`/`*.h` with `clang-format` (prefers `clang-format19` when present, otherwise picks the highest `clang-formatNN` available, otherwise `clang-format`).
- Re-stages any files changed by formatting.
- Runs `cppcheck` on the staged C/C headers with:
  - `--enable=warning,style,performance,portability`
  - `--inconclusive --force`
  - `--std=c11`
- Fails if `cppcheck` reports any `error:` or `warning:` lines (style-only findings do not fail the run).

## Notes

- Uses repo `.clang-format` automatically.
- If your project needs extra include paths/defines for fewer false positives, edit the script’s `CPPFLAGS`/`INCLUDE_DIRS`.

