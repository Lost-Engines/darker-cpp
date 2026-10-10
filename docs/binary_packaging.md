# Builds, packages and automatic releases

The workflow is [`.github/workflows/build.yml`](../.github/workflows/build.yml).
It builds and tests this matrix once, then publishes those same packages:

| Platform | Runner | Compiler | Configurations |
| --- | --- | --- | --- |
| Linux x86-64 | Ubuntu 24.04 | GCC 14 | Debug, Release |
| Windows x86-64 | Windows 2025 / MSYS2 UCRT64 | GCC | Debug, Release |
| macOS Apple Silicon | macOS 15 | Apple Clang | Debug, Release |

Linux builds Boost 1.90.0 Program_options from a checksum-verified archive;
macOS and Windows use their package managers' Boost. Third-party engine
libraries use the pinned archives in `cmake/dependencies.cmake`.

## Triggers and cost control

Pushes to `master`, pull requests and manual dispatch can run CI. Automatic
runs are filtered to `src/`, `tests/`, `cmake/`, `CMakeLists.txt`,
`CMakePresets.json`, `third_party/`, `scripts/`, `tools/` and workflow files.
README and documentation-only changes do not start builds. Pull requests run
once through the pull-request event, rather than also building every branch push.

Only a push to `master` that changes `src/`, `CMakeLists.txt` or `cmake/`
(excluding `cmake/package.cmake`) is eligible for an automatic release.
These include compiler and linked-dependency changes that alter the executable.
Tests, docs, workflow files, packaging scripts and asset-fetch helper updates
alone do not produce releases. Manual dispatch checks a build without publishing.
The release comparison covers the complete pushed commit range, not just its
last commit, including deletions.

All six jobs must succeed before publishing. Compiler caches are limited to
500 MB per platform/configuration, and Linux's Boost installation is cached.
Packages and test reports are retained as workflow artifacts for seven days.
Publication reuses these artifacts; there is no second release compilation.
Obsolete pull-request runs are cancelled; master pushes have distinct groups.

Each release is tagged `release-<full commit SHA>`. Packages are uploaded to a
draft before it is published, with a `SHA256SUMS` file. Rerunning the workflow
for the same commit updates the same release rather than creating another tag.
After publishing all six replacement packages, the job deletes older published
releases and their assets. Drafts and lightweight Git tags are retained.
Publication is serialised, and an older commit finishing late cannot replace a
newer published descendant. A failed build or upload leaves the existing release
available.

Release publication alone receives `contents: write`; build jobs are read-only.
GitHub's repository settings must permit its workflow token to create releases.

## Package contents and use

Each archive contains a `darker/` directory with:

- `bin/darker` (or `bin/darker.exe`), plus required non-system Windows DLLs;
- the optional Bash asset-fetch helper and its URL/checksum lists;
- documentation, dependency notices and source/build references;
- `BUILD.txt`, recording the commit, platform and configuration.

No retail packs, synthesiser ROMs, saves, reference executable or test binaries
are included. CI runs asset-independent tests; original-pack and ROM-dependent
checks remain available locally when their CMake paths are supplied.

Unpack the archive and launch `bin/darker --data-dir /path/to/darker`, or run it
with the game installation as the current directory. Saves are written beside
the packs, interoperably with the DOS game. AWE32 music requires its separately
supplied sample ROM; the asset helper can fetch it, or `--music=soundblaster_fm`
uses the game's own FM data. Windows users can supply a local installation;
the fetch helper currently requires Bash, curl and sha256sum (for example MSYS2).
On macOS, sha256sum is provided by GNU coreutils.

Linux packages target a modern Ubuntu 24.04-compatible runtime and require the
system windowing and OpenGL drivers. Boost and GCC's C++ runtimes are linked
statically in CI. Windows packages bundle non-system DLLs discovered at install
time. macOS packages target macOS 15 or newer on Apple Silicon, with static
Boost and the system C++ runtime/frameworks. They are command-line archives,
not signed/notarised application bundles or installers.

Debug packages include `--screenshot` and `--seconds`. Release packages omit
both. `--level` remains available and suppresses save writes in either build.

## Local build and staging

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix "$PWD/build-release/stage/darker" --component Runtime
cmake -DPACKAGE_ROOT="$PWD/build-release/stage" \
  -DOUTPUT_DIR="$PWD/build-release/dist" -DPLATFORM=linux-x86_64 \
  -DCONFIGURATION=Release -DCOMMIT="$(git rev-parse HEAD)" -P cmake/package.cmake
```

Use a clean staging directory. Local builds use the libraries/toolchain selected
at configuration time, so their runtime requirements can differ from CI.
For example, inspect Linux dependencies with `ldd stage/darker/bin/darker`.
Configure `DARKER_INSTALL_DIR` and `DARKER_REFERENCE_DIR` for original-pack
integration tests; optional sound-device verification has separate asset paths.
A local install/package operation never publishes a release.

Packaging preserves upstream dependency notices and does not introduce a
first-party licence or ownership claim.
