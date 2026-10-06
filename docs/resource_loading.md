# Original resource loading

`darker_resources` is a platform-independent C++23 library. `archive_set` reads `DARKER.00` through `DARKER.04` from an explicitly supplied directory and returns an owned `std::vector<std::byte>` for an archive/slot ID. It neither executes the DOS binary nor reads the analysis exports at runtime.

## Directory and edition

The original pack directory is embedded in the executable. Pointer table `4534` references five arrays of three-byte little-endian boundaries; resource ID splits into archive `id >> 9` and slot `id & 01FFh`.

`src/resources/directory.h` preserves those boundaries as 164 named archive/slot entries. Generate it from the verified unpacked image:

```sh
python3 tools/generate_resource_directory.py ../analysis/unpacked/image.bin
```

The generator checks the complete image SHA-256 and validates increasing boundaries, archive ends and resource count. It is a development tool, not part of the CMake build. Its output is stored with the source. The implementation initially supports only this edition:

| Pack | Resources | Compressed bytes |
|---|---:|---:|
| DARKER.00 | 79 | 1,195,478 |
| DARKER.01 | 10 | 1,337,498 |
| DARKER.02 | 14 | 1,453,028 |
| DARKER.03 | 45 | 1,402,901 |
| DARKER.04 | 16 | 134,178 |

Runtime validates these sizes and known resource IDs. Size validation is not an authenticity/hash check: a same-size modified pack may decode successfully. Reference comparison establishes exact identity for the tested installation; supporting a different edition requires its own verified directory/profile.

## Decoder

The implementation follows the original decoder through the verified Python reference. It uses the initial `4000h` sentinel, MSB-first control bits in little-endian words, interleaved plain literals, literal runs and overlapping matches. The executable's XOR literal transformation does not apply to these resources.

Control bits are consumed in explicit sequence, avoiding expressions with multiple order-dependent reader calls. Copies proceed forwards byte-by-byte so an overlapping reference can read bytes just emitted. Termination must consume exactly the supplied compressed span, although unused bits of its final control word are allowed. Empty/truncated streams, references before the output start, trailing bytes and expansion beyond the configurable limit are errors.

The default expansion ceiling is 8,000,000 bytes per resource, matching the inspection decoder's guard; it is not a recovered DOS allocation limit. All original resources fit comfortably below it.

## Verification

The verifier is built only with `BUILD_TESTING=ON`; it is a test utility, not a shipped application. From this project's directory:

```sh
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

Without `--reference`, the tool still loads and decodes all resources. With it, every byte is compared, not just sizes or checksums. A mismatch reports the resource and first differing byte. This is a read-only command and does not regenerate references.

To register the integration check with CTest:

```sh
cmake -S . -B build \
  -DDARKER_INSTALL_DIR="$PWD/../darker" \
  -DDARKER_REFERENCE_DIR="$PWD/../analysis/resources"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Normal Catch2 cases use independently constructed tiny streams for sentinel/bit order, plain literal runs, overlapping matches, malformed/truncated data and output limits. Directory invariants are tested without external files. Integration covers all **164 resources / 8,956,602 decoded bytes** against exports already checked by the original machine-code decoder; see [format and native-verification evidence](../../docs/resources-and-city.md).

The original loader's conventional-memory/EMS/XMS caches and allocator are intentionally not reproduced. Consumer-specific parsing, resource reset/lifetime behavior, bitmap/font formats and indexed presentation are subsequent milestones.
