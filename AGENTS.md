# Repository Guidelines

## Project Structure & Module Organization
- `src/`: core C implementation (atlaspacker, binpacker, tilepacker, convexhull, project, exporter, util, file).
- `include/atlaspacker/`: public headers for the library API.
- `tool/`: GUI/editor app sources (C++/ObjC++ with Sokol/ImGui).
- `test/`: C++ tests and fixtures; `test/win32/` contains Windows portability helpers.
- `external/`: vendored third-party sources and wrappers; `external/repos/` is populated by scripts.
- `scripts/`: build helpers and tooling entry points.
- `doc/`, `examples/`, `exporters/`, `build/`, and sample `image_*.tga` assets at repo root.

## Build, Test, and Development Commands
- `./scripts/install_deps.sh`: clone third-party repos into `external/repos/` (requires git; imgui/xxHash use SSH URLs).
- `./scripts/compile_deps.sh`: build static libs into `build/`.
- `./scripts/compile_lib.sh`: build `build/libatlaspacker.a`.
- `./scripts/compile_editor.sh`: build GUI tool at `build/tool/atlaspackergui` (macOS frameworks).
- `./scripts/compile_test.sh`: build test binaries (currently `build/test_project`; others are commented).
- Run tests with `./build/test_project`.
- Useful env vars: `USE_ASAN=1`, `USE_TSAN=1`, `OPT=-O0 -g`, `STDCVERSION=c11`, `STDCXXVERSION=c++14`.

## Coding Style & Naming Conventions
- Indent with 4 spaces; braces on their own line; keep style consistent with `src/atlaspacker.c`.
- C sources in `src/` use `.c`; C++ tooling/tests use `.cpp`.
- Public API types/functions use the `ap` prefix; filenames are lowercase with underscores.
- Avoid C++ exceptions (compile flags disable them).
- C/C++: Use repo `.clang-format` (LLVM base, custom Allman). Run your editor integration or `clang-format -i` before committing.
- Braces/indentation: Allman style (braces on new lines for functions, control statements, namespaces). 4 spaces; no tabs.
- Pointers/refs: Left‑aligned, e.g. `Type* ptr`, `Type& ref`. Space before control parens (`if (`), none for calls.
- C++ features: C++11; no exceptions or RTTI in engine code; prefer error codes (`Result`) and `assert` for invariants.
- Includes: Local headers first with quotes, then SDK/engine headers with angle brackets. Don’t auto‑sort; group logically.
- Naming: Filenames lower_snake_case. Macros and constants UPPER_SNAKE_CASE. Public C API prefixed `dm...`; handle types `HType`.
- Line length: No hard limit; keep lines readable and wrap at sensible boundaries.
- Files/dirs: Tests produce `test_*` binaries; follow existing per‑module patterns.
- Favor a C‑first design (POD structs + free functions in `dm` namespaces). Expose opaque handles (`HType`) in public headers; keep implementation types private.
- Lifecycle: Use explicit `Create/Destroy` and `Initialize/Finalize` for modules and resources; avoid hidden global state.
- Memory & perf: Avoid allocations in hot paths; pre‑allocate C arrays if necessary.
- Templates/virtuals: Keep templates minimal; avoid inheritance/virtual dispatch in engine runtime; prefer composition and data‑oriented layouts.


## Testing Guidelines
- Tests use `external/jc_test.h` with `test_*.cpp` naming.
- Add new tests under `test/` and wire them into `scripts/compile_test.sh`.
- No coverage target documented; include regression tests for new packer behavior and data file handling.

## Commit & Pull Request Guidelines
- Commit messages in history are short, sentence-style (e.g. "Added ...", "removed ..."). Follow that tone without scopes.
- PRs should describe the change, include build/test commands run, and add screenshots/GIFs for GUI changes.

## Dependency & Configuration Notes
- `external/repos/` is expected for builds; if dependencies are missing, run `./scripts/install_deps.sh`.
- `scripts/compile_deps.sh` runs `make` in `external/repos/xxHash`; ensure build tools are installed.
