# Building and staging a Linux binary

The installation contains one executable, documentation with its small validation
reports/launch chart, and the
third-party notices. It contains no original game packs, pilot saves, extracted
assets or verification executables. It does not add a project licence or claim
ownership of the original game.

From the repository root:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix "$PWD/build-release/stage" --component Runtime
```

To include original-pack integration checks, supply `DARKER_INSTALL_DIR` and
`DARKER_REFERENCE_DIR` at configuration time, pointing to the retail install
and independently decoded resource directory. These are verification inputs,
not installed resources. Use a clean staging directory for every package.

Run the staged `bin/darker` from a directory containing `DARKER.00` through
`DARKER.04`, or supply `--data-dir`. Saves belong to the working directory.
The application still accepts all documented debugging switches; `--level`
suppresses save writes.

## Runtime dependencies

This is a distribution-specific binary, not a self-contained portable bundle.
The build links Boost.Program_options, the C/C++ runtimes and platform OpenGL
libraries. GLFW and miniaudio also load window/audio system libraries at runtime.
Check `ldd build-release/stage/bin/darker` and test the staged executable on the
intended modern Linux environment. The current workstation build needs
Boost.Program_options 1.90.0 and exports a requirement for `GLIBCXX_3.4.36`.
Older-distribution compatibility is not a project target. These are local
build and staging instructions; release automation and CI are separate work.

The selected OpenGL path requires a suitable graphics driver. Test both the
ordinary hardware path and a software-rendered window when available. Use a
separate working directory for any menu/save smoke test.

## Dependency source references

The installed `third_party/source-references/dependencies.cmake` records the exact
GitHub archives and SHA-256 hashes for GLFW, miniaudio, TinySoundFont, Munt, Nuked-SC55, Nuked OPL3 and DOSBox DBOPL.
`dbopl.cmake` and the compatibility `dosbox.h` describe the adapter extraction
used at build time. Their original licence texts are installed alongside them;
Boost's notice is included separately. Catch2 is a test-only dependency and is
not linked into the installed executable.

Maintainers handle release publication and licensing decisions separately.
Staging a local installation does not publish a release or create a Git tag.
