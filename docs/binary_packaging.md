# Builds, packages and automatic releases

The workflow is [`.github/workflows/build.yml`](../.github/workflows/build.yml).
It builds and tests this matrix once, then publishes only the Release packages:

| Platform | Runner | Compiler | Configurations |
| --- | --- | --- | --- |
| Linux x86-64 | Ubuntu 24.04 | GCC 14 | Debug, Release |
| Windows x86-64 | Windows 2025 / MSYS2 UCRT64 | GCC | Debug, Release |
| macOS Apple Silicon | macOS 15 | Homebrew GCC | Debug, Release |

Linux and macOS build Boost 1.90.0 Program_options from a checksum-verified archive;
Windows uses MSYS2’s static Boost. macOS builds Boost with GCC to match the
engine’s C++ ABI. Third-party engine
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
500 MB per platform/configuration, and the Boost installations are cached.
Packages and test reports are retained as workflow artifacts for seven days.
Publication reuses the three Release artifacts; Debug packages remain available
as CI artifacts only. There is no second release compilation.
Obsolete pull-request runs are cancelled; master pushes have distinct groups.

Each release is tagged `release-<full commit SHA>`. Packages are uploaded to a
draft before it is published, with a `SHA256SUMS` file. Rerunning the workflow
for the same commit updates the same release rather than creating another tag.
After publishing all three replacement packages, the job deletes older published
releases and their assets. Drafts and lightweight Git tags are retained.
Publication is serialised, and an older commit finishing late cannot replace a
newer published descendant. A failed build or upload leaves the existing release
available.

Release publication alone receives `contents: write`; build jobs are read-only.
GitHub's repository settings must permit its workflow token to create releases.

## Package contents and use

Published archives are named `darker-linux.tar.gz`, `darker-windows.zip` and
`darker-macos.tar.gz`. CI artifact names identify architecture and configuration.

Each archive contains a `darker/` directory with only:

- `darker` (or `darker.exe`) at its top level;
- `scripts/`, copied unchanged from the source tree, including `fetch-assets.sh`
  and its URL/checksum files.

Documentation and dependency information remain in the source repository.
Game packs, saves, ROMs, soundfonts, patch banks and developer tools are not bundled.

Unpack the archive and launch `./darker --data-dir /path/to/darker`, or run it
with the game installation as the current directory. Saves are written beside
the selected packs. The optional `scripts/fetch-assets.sh` helper requires Bash,
curl and sha256sum. On macOS, sha256sum is provided by GNU coreutils.

Linux packages target a modern Ubuntu 24.04-compatible runtime and require the
system windowing and OpenGL drivers. Boost and GCC's C++ runtimes are linked
statically in CI. Windows packages statically link all non-system libraries.
The executable is checked with only Windows system directories on PATH.
macOS packages target macOS 15 or newer on Apple Silicon, with static Boost
and GCC C++ runtimes, and system frameworks. They are command-line archives,
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
  -P cmake/package.cmake
```

Use a clean staging directory. Local builds use the libraries/toolchain selected
at configuration time, so their runtime requirements can differ from CI.
`DARKER_STATIC_RUNTIME` defaults to on for MinGW and is enabled on all CI builds.
It links non-system dependencies statically on Windows and GCC's C++ runtimes
statically on Linux/macOS. macOS uses GCC for C++ and the final link; only the
Cocoa and CoreAudio device implementations use Apple's Objective-C compiler.
For example, inspect Linux dependencies with `ldd stage/darker/darker`.
Configure `DARKER_INSTALL_DIR` and `DARKER_REFERENCE_DIR` for original-pack
integration tests; optional sound-device verification has separate asset paths.
A local install/package operation never publishes a release.

Packaging does not introduce a first-party licence or ownership claim.
