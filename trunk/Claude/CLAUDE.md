# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

GNU APL — a free C++ implementation of ISO/IEC 13751 ("Programming Language APL, Extended"). The upstream VCS is Subversion (see `.svn/`, `exported_from`); this tree is not a git repo. Maintainer: Dr. Jürgen Sauermann.

## Build and test

Built with GNU Autotools. The default (in-tree) build produces the `src/apl` interpreter:

```sh
./configure              # regenerate with autoreconf if configure.ac changed
make                     # produces src/apl
make -C src test         # run all testcases, continue past errors
make -C src test1        # run all testcases, stop on first error (--TM 3)
make -C src test2        # run with --TR (randomized order)
make -C src testAP       # run only AP-related testcases
make -C src perf         # run performance testcases
```

Testcases live in `src/testcases/*.tc`. To run a single one:

```sh
cd src && ./apl --id 1010 -T testcases/Add.tc
```

`--id 1010` assigns a processor ID (needed because tests may use shared variables / APs). `--TM 3` stops on first error; `--TR` randomizes ordering.

Pre-release sanity checks (run from top level):

```sh
make check_sources       # tools/check_src + tools/check_command_def
```

### Configure-time variants

GNU APL has many `./configure` knobs; the important ones (all documented in `README-2-configure`):

- `--with-libapl` — build `libapl.so` instead of the `apl` binary (library API in `src/libapl.h`; entrypoint `init_libapl()`).
- `--with-python` — build `lib_gnu_apl.so` (Python binding via `src/python_apl.cc`).
- `--with-erlang` — build the Erlang NIF (see `erlang/`).
- `--without-gtk3` — force XCB backend for `⎕PLOT` even if GTK3 is present. With GTK3 enabled, `src/Gtk/` is also built (supports `⎕GTK`).
- `--with-sqlite3[=DIR]` / `--with-postgresql[=DIR]` — enable `⎕SQL` against the respective backend (files under `src/sql/`).
- `DEVELOP_WANTED=yes`, `RATIONAL_NUMBERS_WANTED=yes`, `CORE_COUNT_WANTED=N|syl|-N`, `PERFORMANCE_COUNTERS_WANTED=yes`, `DYNAMIC_LOG_WANTED=yes`, `VALUE_CHECK_WANTED=no`, `VALUE_HISTORY_WANTED=no`, `ASSERT_LEVEL_WANTED=0|1|2`, `SECURITY_LEVEL_WANTED=0|1|2`.

`build/build_all` is the nightly-build driver: it performs VPATH builds of several preset configurations (`standard`, `develop`, `libxcb`, `rational`, `libapl`, `parallel_bench`, `erlang`, `python`) into `build/builddir_<name>/`. Read this script to see canonical flag combinations for each variant.

### VPATH builds

To build multiple configurations side-by-side without recompiling from a polluted tree, use VPATH builds (see `README-12-vpath-builds`):

```sh
mkdir obj && cd obj && ../configure <opts> && make
```

If a non-VPATH build has already run in this tree, run `make VPATH_clean` first, otherwise stale `src/*.o` files will be silently reused.

## Architecture

The interpreter is one monolithic C++ program. `src/main.cc` provides `main()`, and `src/Makefile.am`'s `common_SOURCES` lists all files that are linked into `apl`, `libapl.so`, and `lib_gnu_apl.so` alike.

### Execution pipeline

1. **Input** — `LineInput.cc`, `InputFile.cc`, `UserPreferences.cc` read lines from stdin, `-f` script files, or `-T` testcases. `TabExpansion.cc` handles readline completion.
2. **Tokenizer** (`Tokenizer.cc`) → tokens defined in `Token.def` / `TokenEnums.hh`.
3. **Parser** (`Parser.cc`) does surface-level parsing (literals, indexing brackets, etc.) into a `Token_string`.
4. **Prefix reducer** (`Prefix.cc`, phrases in `Prefix.def`) is the actual APL evaluator: a shift/reduce stack machine that matches up to `MAX_PHRASE_LEN` (4) tokens against reduction phrases and rewrites the stack. (`MAX_REDUCTION_LEN = 4` in `Prefix.hh` is the same, used when Prefix.def is not #included — it only sizes the lookahead buffer as `MAX_CONTENT = 10*MAX_REDUCTION_LEN`.) Each `reduce_XXX()` returns an `R_action` (`RA_CONTINUE`, `RA_PUSH_NEXT`, `RA_RETURN`, `RA_SI_PUSHED`).
5. **State Indicator stack** (`StateIndicator.cc`) — APL's equivalent of a C++ call stack; one `StateIndicator` per user-function / ⎕EA / ⎕EC / ⍎ invocation. The top-level `Workspace` (`Workspace.cc`) holds the SI chain plus the two symbol tables (user names in `SymbolTable`, distinguished/quad names in `SystemSymTab`).
6. **Values** — `Value.hh` defines an APL array as a `Shape` + ravel of `Cell`s. `Cell` is an abstract tagged union; concrete subtypes are `CharCell`, `IntCell`, `FloatCell`, `ComplexCell`, `PointerCell` (for nested arrays), `LvalCell` (assignment targets). `Value_P` is the reference-counted smart pointer; values are never constructed directly, only via `Value_P`.

### Functions, operators, system names

- `Bif_F12_*.cc` / `Bif_OPER{1,2}_*.cc` — built-in primitive functions and operators (each file is one glyph or family: `COMMA`, `DOMINO`, `EACH`, `INNER`, `RANK`, etc.).
- `ScalarFunction.cc` — scalar-function dispatch (parallelizable; see `Parallel.cc`, `PJob.hh`, `Thread_context.cc`).
- `Quad_*.cc` — each system function `⎕XX` (e.g. `⎕CR`, `⎕FIO`, `⎕SQL`, `⎕FFT`, `⎕PLOT`, `⎕JSON`, `⎕XML`, `⎕RE`, `⎕GTK`). The full set is declared in `SystemVariable.def` / `QuadFunction.cc`.
- `Command.cc` / `Command.def` — `)XXX` user commands (`)LOAD`, `)SAVE`, `)WSID`, …).
- `Id.def` — the central X-macro table of every identifier (quad-name, command, error) known to the interpreter; included many times with different `id_def`/`qv_def`/etc. macros. Editing identifiers means editing `.def` files, not scattered switch statements.

### Other `.def` X-macro files to know

Many subsystems are table-driven via X-macros. When changing one of these areas, edit the `.def` file:

- `Error.def` — every error code and message (also consumed by `libapl.h`).
- `Command.def` — `)XXX` command table.
- `Logging.def` — logging facilities; `check_src` enforces that all are shipped off (`log_def(0, …)`).
- `Macro.def` — internal APL "macro" functions compiled from source strings at startup.
- `Performance.def`, `SystemLimits.def`, `Help.def`, `Avec.def` (APL character set), `Prefix.def` (reduction phrases), `Svar_signals.def`, `Quad_CR.def`, `Quad_FIO.def`, `Quad_MX.def`, `Quad_PLOT.def`, `Gtk/Gtk_map.def`, `Gtk/Gtk_enum_map.def`.

### Subsystems built as separate binaries/libraries

- `src/APs/` — Auxiliary Processors. `APserver` is the shared-variable broker (auto-started, transport configured via `APSERVER_TRANSPORT` / `APSERVER_PORT` / `APSERVER_PATH` at configure time). `AP100`, `AP210` are example APs.
- `src/native/` — template for user-written native APL functions (loaded via `⎕FX`'s native form). See `template_F12.cc` etc.
- `src/Gtk/Gtk_server.cc` — separate GTK3 helper process for `⎕GTK`.
- `src/sql/` — SQLite / Postgres connection glue for `⎕SQL` (compiled only when the respective backend is configured in).

### Workspaces and library paths

- `workspaces/`, `wslib3/`, `wslib4/`, `wslib5/` (top level) — shipped APL workspace libraries (`)LIB 3`, `)LIB 4`, `)LIB 5`). `LibPaths.cc` computes the search order.
- `src/workspaces/` — workspaces used by the testcases.
- `gnu-apl.d/preferences` — installed system preferences; user overrides go under `$HOME/.config/gnu-apl/` (see `README-9-post-installation`).

## Conventions

- C++ style uses `Assert()` / `Assert1()` macros (`Assert.hh`); `Assert1` is a build-time-disabled version for hot paths (see `ASSERT_LEVEL_WANTED`).
- Most classes carry a `const char * loc` parameter and use the `LOC` macro (file:line string) for value-tracking diagnostics (see `ValueHistory.cc`, `libapl.h`).
- Before committing, all logging facilities in `Logging.def` must be off and debug `Q()` macros must be removed; `tools/check_src` enforces this and is invoked by `make check_sources` / `EXPO`.
- Packaging targets (`DIST`, `DEB`, `RPM`, `EXPO`, `SYNC`) at the top level assume a Savannah workflow and SVN commit access — don't invoke them casually.
