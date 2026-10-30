# Contributing

Bug reports, fixes, enhancements and format research are welcome. This page covers how to build, what gets merged, and how to submit it.

## Scope

- The default experience matches the original game. Framerate independence is the one intended change.
- Enhancements are opt-in settings, off by default unless they don't change gameplay.
- Original bugs stay unless they crash the game or depend on framerate.
- Game files are never committed. The game reads them from the player's install at runtime.
- racers-rebuilt is a reimplementation. Facts learned from the original (constants, data layouts, call order) are fine. Copied or paraphrased decompiled code, binary addresses and decompiler names aren't.

Open a feature request before starting on a large change, so the scope is agreed first.

## Building

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Windows | 10 or 11, 64-bit | The only supported platform |
| Visual Studio | 2022, or its Build Tools | The **Desktop development with C++** workload (MSVC v143 and the Windows SDK) |
| CMake | 3.28 or later | |
| Ninja | Any recent | |
| vcpkg | A clone of [microsoft/vcpkg](https://github.com/microsoft/vcpkg) | Set `VCPKG_ROOT` to its folder |
| LLVM | Any recent release | `clang-format` and `clang-tidy`, used by `scripts/check.ps1` |

Shaders compile at build time with `dxc`, which ships with the Windows SDK. Build from a **Developer PowerShell for VS 2022** so `cl`, `dxc` and the SDK are on `PATH`.

### Build and run

```powershell
git clone https://github.com/chrismacdonaldw/racers-rebuilt.git
cd racers-rebuilt
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
.\build\debug\bin\racers-rebuilt.exe
```

vcpkg installs the dependencies from `vcpkg.json` on the first configure.

| Preset | Build | Tests and tools |
|---|---|---|
| `debug` | Debug | Built |
| `release` | Release, LTO, one static exe | Not built |
| `check` | Release with asserts on | Built. Used by the merge gate. |

Output goes to `build/<preset>/bin/`.

### Tests with real game data

Tests that read the original files skip unless `RR_GAME_PATH` points at a LEGO Racers install.

```powershell
$env:RR_GAME_PATH = "C:\Program Files (x86)\LEGO Media\Games\LEGO Racers"
ctest --preset debug
```

### Recorded test race

`tests/data/replays/test_track.replay` is a race on a test track built in code, and its `.hashes` file holds every tick's snapshot hash. A change that alters the simulation on purpose breaks it. Re-record it, check that the new behaviour is the one you meant, and commit both files:

```powershell
$env:RR_RECORD_TEST_REPLAY = "1"
.\build\debug\bin\racers-rebuilt-tests.exe -tc="the recorded test track*"
Remove-Item Env:RR_RECORD_TEST_REPLAY
```

## Merge gate

There's no CI. `scripts/check.ps1` is the gate, and every PR needs a passing run on its head commit.

```powershell
$env:RR_BANNED_TERMS_FILE = "C:\path\outside\the\repo\banned-terms.txt"
.\scripts\check.ps1
```

It runs on a clean working tree, loads the MSVC environment if needed, and stops at the first failure:

1. No uncommitted changes
2. No game data or ignored files tracked
3. No banned terms in the tree, commit messages or branch name
4. Toolchain present
5. Configure the `check` preset into an empty build folder
6. Build
7. Run the tests
8. `clang-format --dry-run --Werror` on every source file
9. `clang-tidy` on every source file

`RR_BANNED_TERMS_FILE` names a text file outside the repo with one regular expression per line. Ask a maintainer for the current list.

Paste the summary it prints into the PR's **Testing** section, with the commit hash.

## Project layout

- `src/core/`: `Result` and errors, logging, assertions, math types
- `src/platform/`: SDL setup, window, events, timers, file mapping, folder picker, data folder. Win32 code lives only in `src/platform/win32/`.
- `src/assets/`: the virtual file system, `LEGO.JAM` reader and file format parsers
- `src/settings/`: setting declarations, `settings.toml` and command-line overrides
- `src/sim/`: fixed-tick simulation state, input frames and render snapshots
- `src/render/`: the SDL GPU renderer
- `src/overlay/`: the <kbd>F1</kbd> settings overlay, built on Dear ImGui
- `src/audio/`: mixer, ALP decoder and music player
- `src/mods/`: mod discovery, hooks and enhancements
- `src/app/`: startup, the frame loop and scenes
- `tests/<module>/`: unit tests, mirroring `src/`
- `shaders/`: HLSL, compiled and embedded at build time
- `tools/`: developer tools, not shipped
- `scripts/`: `check.ps1` (the merge gate), `package.ps1` (builds `racers-rebuilt-windows-x64.zip`) and `msvc.ps1` (loads the MSVC environment)

Each module is a static library. A module links only the modules below it. `sim` never touches SDL, the GPU, audio, settings or a clock.

## Code style

- C++23 as implemented by MSVC 2022. No modules.
- `.clang-format` and `.clang-tidy` at the repo root are the authority. The merge gate fails on any diff or warning.
- Code never throws. Fallible functions return `rr::Result<T>`, and every result is handled or propagated.
- No singletons or mutable globals. `src/app/` builds every system and passes dependencies in.
- No heap allocation per tick or per frame in `sim`, `render` or `audio` hot paths.
- Source files stay under 500 lines and functions under 60.

### Naming

| Thing | Style | Example |
|---|---|---|
| Namespaces | `rr::<module>` | `rr::assets` |
| Types and functions | PascalCase | `JamArchive`, `LoadTrack()` |
| Variables and parameters | camelCase | `tickCount` |
| Private members | camelCase with a trailing `_` | `accumulator_` |
| Constants | `k` + PascalCase | `kSimTick` |
| Files | snake_case after the main type | `jam_archive.hpp` |
| Tests | `<file>_test.cpp` | `jam_archive_test.cpp` |

- Units go in names or types: `tickMs`, `std::chrono::nanoseconds`.
- Booleans read as predicates: `isLoaded`, `hasOverride`.
- No `utils`, `helpers`, `misc`, `common`, `manager` or `base` in file or type names.

### Comments

The default is no comment. Names, types and structure carry the meaning.

- A `///` doc comment is allowed only when the signature can't say what a caller needs. That covers preconditions, units, ownership, threading, and surprising caps or fallbacks. Keep it to one or two lines.
- Never on enum values, fields, constructors, overrides, or functions and types whose name says what they do.
- No `//` comments, `TODO`s or commented-out code. Open an issue instead.
- Tool directives (`// NOLINTNEXTLINE(<check>)`, `// clang-format off`) are the only exception.

## Commits and branches

Branch from `main` with a prefix and a kebab-case scope: `feat/music-crossfade`. The prefixes are `feat/`, `fix/`, `refactor/`, `chore/` and `docs/`.

Commits follow [Conventional Commits](https://www.conventionalcommits.org/): `type(scope): summary`. Lowercase, imperative, no period, 72 characters at most. The scope is the module.

| Type | For |
|---|---|
| `feat` | New behavior or setting |
| `fix` | A bug fix |
| `perf` | A speedup, with before and after numbers in the PR |
| `refactor` | A change with no behavior change |
| `test` | Tests only |
| `build` | CMake, presets, vcpkg, scripts |
| `docs` | README, CONTRIBUTING, doc comments |
| `chore` | Anything else |

- No commit trailers.
- Breaking changes use `!` and a `BREAKING CHANGE:` footer.
- Versions follow [SemVer](https://semver.org/). The version lives only in `project(VERSION)` in the root `CMakeLists.txt`.

## Pull requests

- One feature or fix per PR, roughly 100 to 1500 changed lines.
- The title is the conventional commit header. PRs are squash-merged with that title.
- Fill in every section of the PR template. Link the issue with `Closes #123`.
- Every bug fix adds a regression test. Every parser and simulation system has unit tests.
- Gameplay changes say which original behavior they reproduce and how it was confirmed.

## Reporting bugs

Use the **Bug report** form. Include:

- The racers-rebuilt version
- Windows version, GPU and display refresh rate
- The log from `<data>/logs/racers-rebuilt.log`, copied before relaunching
- Whether the original game does the same thing

## Developer tools

| Tool | Use |
|---|---|
| `racers-rebuilt.exe --dump-images <folder>` | Decodes every `BMP` and `TGA` in the game data to PNG, then exits |
| `jam-dump <LEGO.JAM>` | Lists every file in the archive with its size |
| `jam-dump <LEGO.JAM> <folder>` | Extracts the whole archive |
| `--set dev.play_sound=<path>` | Plays one sound once audio starts, e.g. `GAMEDATA/COMMON/321.PCM` |

`jam-dump` builds with the `debug` and `check` presets. Write output to `dumps/`, which is ignored by git. Never commit anything extracted from the game.
