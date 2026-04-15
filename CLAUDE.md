# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**mport** is the MidnightBSD package manager, written in C for MidnightBSD 3.0+. It handles package installation, upgrades, deletion, verification, auditing, and dependency resolution. Current version: 2.7.7, DB master schema v14, bundle schema v6.

## Build Commands

Uses BSD make (`make` or `bmake`):

```sh
make          # Build all targets (library, CLI, libexec tools)
make clean    # Clean build artifacts
```

Build is split across subdirectories — the root `Makefile` dispatches to `libmport/`, `mport/`, `libexec/`, and `liblua/`. To build just one component:

```sh
cd libmport && make
cd mport && make
cd libexec/mport.install && make
```

The code compiles with `-Werror`, so all warnings must be resolved.

## Code Formatting

`.clang-format` is present — WebKit-based style:
- **Indentation**: tabs, 8-space tab width
- **Column limit**: 100
- **Pointer alignment**: Right

Format changed files with `clang-format -i <file>`.

## Testing

There is no automated test suite. CI runs via Jenkins (`Jenkinsfile`, matrix builds on amd64/i386) and GitHub Actions CodeQL (`.github/workflows/c-cpp.yml`). Testing is manual and integration-based. Correctness validation is primarily compile-time (strict warnings as errors).

## Architecture

Three-tier design:

### 1. `libmport/` — Core Library

Compiled as `libmport.so.2` and `libmport.a`. Contains ~45 C source files implementing all package management logic.

- **Public API**: `libmport/mport.h` — all external-facing types and functions
- **Internal API**: `libmport/mport_private.h` — shared only within the library

Key source files:
| File | Role |
|------|------|
| `db.c` | All SQLite operations; schema creation/migration (master schema v14) |
| `install_primative.c` | Core install: asset extraction, permissions, checksums |
| `delete_primative.c` | Package removal; checks reverse dependencies before deletion |
| `bundle_read_install_pkg.c` | Reads `.mport` bundle format (archive + metadata) |
| `fetch.c` | Network downloads via libfetch |
| `index.c` | Remote package index queries and caching |
| `pkgmeta.c` | Package metadata queries against the local DB |
| `lua.c` | Lua 5.4 hook execution (pre/post-install scripts) |
| `audit.c` | CVE audit via CPE identifiers |
| `verify.c` | File checksum verification |
| `version_cmp.c` | Package version comparison algorithm |
| `util.c` | String helpers, ELF binary inspection, hashing |
| `plist.c` | Package plist file parsing |

**Core data structures** (defined in `mport.h`):
- `mportInstance` — main context: DB handle, callbacks, verbosity, online/offline state
- `mportPackageMeta` — package record: name, version, origin, categories, CPE, deps
- `mportAssetList` — list of files/scripts in a package with asset type (FILE, DIR, EXEC, PREEXEC, POSTEXEC, etc.)
- `mportIndexEntry` — entry from the remote package index
- `mportDependsEntry` — dependency relationship record

### 2. `mport/` — Main CLI Tool

Single binary at `/usr/sbin/mport`. Entry point: `mport/mport.c`. Parses subcommands (install, delete, update, upgrade, search, audit, verify, info, list, etc.) and dispatches to libmport functions or libexec tools.

### 3. `libexec/` — Backend Utilities

14 focused executables installed to `/usr/libexec/`, each handling a discrete operation. They are invoked by the main CLI and link against `libmport`. Notable ones:

- `mport.install` — installation primitives
- `mport.delete` — package deletion
- `mport.update` / `mport.upgrade` — package updates
- `mport.create` — package bundle creation
- `mport.fetch` — download packages from mirrors
- `mport.merge` — merge package databases
- `mport.query` / `mport.list` / `mport.info` — querying
- `mport.version_cmp` — version comparison
- `mport.updepends` — update dependency records

## External and Embedded Libraries

**Embedded** (compiled into the project):
- `external/lua/src/` — Lua 5.4 VM, built as `liblua.a` via `liblua/`
- `external/tllist/tllist.h` — header-only intrusive linked list (TAILQ/STAILQ-style)

**System libraries** linked at build time:
- `libarchive` — archive extraction/creation
- `sqlite3` — package registry database
- `libfetch` — HTTP/FTP downloads
- `libelf` — ELF binary inspection
- `libucl` — UCL configuration (private copy)
- `libzstd` — Zstd compression (private copy)
- `libmd` — cryptographic hashing (SHA256, etc.)
- `liblzma`, `libz` — additional compression

## Package Database

SQLite database lives at `/var/db/mport/`. Schema versioning is managed in `libmport/db.c`. The `mportInstance` struct holds the open DB handle. Both a "master" DB (installed packages) and temporary "stub" DBs (for bundle operations) are used.

## Install Workflow (high level)

1. `mport install <pkg>` → CLI loads `mportInstance`, queries index
2. Downloads `.mport` bundle (libarchive tar + SQLite metadata) via `mport.fetch`
3. Extracts and validates metadata; checks dependencies and conflicts
4. Deploys assets via `mport.install` / `install_primative.c`
5. Runs Lua pre/post-install hooks via `libmport/lua.c`
6. Updates master SQLite registry
