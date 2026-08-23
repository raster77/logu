# Building log-utils

CMake is the only supported build: it's cross-platform, and downloads the
dependencies that aren't vendored in `libs/` automatically.

Note that the built binary is named **`logu`** — the CMake target and
executable are `logu`, while `log-utils` is the project's name.

The repo also still carries Eclipse CDT project files
(`.project`/`.cproject`/`.settings/`) and a leftover `Release/` directory
holding old object files and a built `logu.exe`, from a managed build that
predates CMake. Its generated makefiles are gone and there is no `Debug/`,
so that build no longer works; ignore those files.

## Requirements

- A C++20 compiler: GCC, Clang, or MSVC.
- [CMake](https://cmake.org/) 3.21 or newer.
- Git (used by CMake's `FetchContent` to download dependencies).
- A build tool: Ninja, Make, or Visual Studio/MSBuild.
- On Linux, clip (the clipboard library) needs X11/xcb development headers
  installed — e.g. on Debian/Ubuntu: `sudo apt install libxcb1-dev`.

## Build

```
cmake -S . -B build
cmake --build build -j
```

The resulting binary is always at `build/bin/logu` (or `build/bin/logu.exe`
on Windows) — `CMakeLists.txt` pins the output directory to `build/bin` for
every generator and config, including multi-config generators like Visual
Studio, so this path doesn't change based on how you configured the build.
`formats.json` is copied next to it automatically (see below).

Sources are collected with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` over
`src/**/*.cpp`, so adding a new source file needs no edit to
`CMakeLists.txt`.

To use a specific generator or build type:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## What CMake downloads

`libs/cli11`, `libs/mio`, and `libs/nlohmann` are vendored, header-only
libraries already checked into the repo — nothing to fetch for those.

For the rest, `CMakeLists.txt` uses `FetchContent` to clone and build from
source, so a fresh checkout builds correctly on any machine without manual
setup:

| Library | Source | Notes |
| --- | --- | --- |
| zlib | `find_package(ZLIB)` first (uses the system copy if present, common on Linux/macOS); falls back to fetching [madler/zlib](https://github.com/madler/zlib) v1.3.1 otherwise (typical on Windows). |
| FTXUI | Always fetched from [ArthurSonzogni/FTXUI](https://github.com/ArthurSonzogni/FTXUI) (pinned tag, currently v7.0.3). |
| clip | Always fetched from [dacap/clip](https://github.com/dacap/clip) (tracks `main` — upstream has no stable release tags). |

The first configure clones these into `build/_deps/` and can take a few
minutes; subsequent configures/builds reuse what's already there.

## formats.json

`CMakeLists.txt` copies the repo's `formats.json` to `build/bin/formats.json`
as part of the default build (via a dedicated `logu-formats` target, not a
post-link step, so it's re-checked on every build invocation — not just when
`logu` itself gets relinked). This is purely a convenience:
`TimestampFormatCatalog` falls back to built-in defaults if the file is
missing (see `service/TimestampFormat.cpp`), so it's safe to delete or
ignore.

## Static builds on Windows

On Windows, the executable is linked statically by default so it doesn't
depend on any MinGW runtime DLLs (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`,
`libwinpthread-1.dll`) or a matching MSVC redistributable being installed on
the target machine:

- **MinGW/GCC**: linked with `-static -static-libgcc -static-libstdc++`.
- **MSVC**: uses the static CRT (`/MT` / `/MTd`) via
  `CMAKE_MSVC_RUNTIME_LIBRARY`.

This is handled automatically in `CMakeLists.txt` — no extra flags needed.
On Linux/macOS this doesn't apply and the build behaves like a normal CMake
project.

## Tests

The unit tests build alongside the binary and are registered with CTest:

```
ctest --test-dir build --output-on-failure
```

They can also be run directly as `build/bin/logu-tests`. Pass
`-DLOGU_BUILD_TESTS=OFF` at configure time to skip building them.

They cover the non-terminal parts of the code (timestamp parsing and format
detection, merge/dedup, the document model, filter expressions, the command
layer, the file/directory helpers). The viewer itself isn't covered — see
below.

## Verifying a build

The viewer has no automated coverage, so after building, exercise it
manually:

```
build/bin/logu --help
build/bin/logu --dedup FILE1 FILE2 -o merged.log
```

or open the interactive viewer against an already-merged log:

```
build/bin/logu --view merged.log
```

The repo ships sample logs to exercise it against: `logs/s-rec-gue/prd-01`
and `logs/s-rec-gue/prd-02` (two directories of real logs, also useful for
`--working-dir`), and a pre-merged `Release/test/merged.log` for `--view`.
