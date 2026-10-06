#!/usr/bin/env python3
"""Generate engine rasterisation checksums from native captures, never runtime animation tables."""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def checksum(pixels):
    result = 14695981039346656037
    for value in pixels:
        result = ((result ^ value) * 1099511628211) & ((1 << 64) - 1)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'tests/reference/procedural_samples.h')
    args = parser.parse_args()
    source = args.workspace / 'analysis/interface/dynamic/procedural.json'
    markers = args.workspace / 'analysis/interface/dynamic/dynamic-hud.json'
    data = json.loads(source.read_text())
    lines = ['#pragma once', '', '// Generated from original-code raster captures by generate_procedural_reference.py',
             '// Procedural SHA-256: ' + hashlib.sha256(source.read_bytes()).hexdigest(),
             '// Markers SHA-256: ' + hashlib.sha256(markers.read_bytes()).hexdigest(), '',
             '#include <array>', '#include <cstdint>', '#include "graphics/procedural_hud.h"', '', 'namespace darker::test_reference {', '',
             'struct attitude_sample {', '  std::uint16_t pitch;', '  std::uint16_t roll;', '  std::uint64_t checksum;', '};', '',
             f'inline std::array<attitude_sample, {len(data["attitude"]["frames"])}> constexpr attitude{{{{']
    for frame in data['attitude']['frames']:
        pixels = bytearray(320 * 240)
        for x, y, length in frame['runs']:
            pixels[(y+8)*320+x:(y+8)*320+x+length] = bytes([14]) * length
        lines.append(f'  {{.pitch{{{frame["pitch"] % 1024}}}, .roll{{{frame["roll"] % 1024}}}, .checksum{{0x{checksum(pixels):016x}ULL}}}},')
    lines += ['}};', '', 'inline std::array<std::uint64_t, 3> constexpr markers{']
    for marker in json.loads(markers.read_text())['reticles']:
        pixels = bytearray(320 * 240)
        for x, y, colour in marker['pixels']:
            pixels[(y+84)*320+x+160] = colour
        lines.append(f'  0x{checksum(pixels):016x}ULL,')
    lines += ['};', '']
    sys.path.insert(0, str(args.workspace.resolve() / 'tools'))
    from procedural_hud import Attitude, native_line
    pairs = [((10, 10), (10, 10)), ((0, 20), (319, 20)), ((0, 20), (319, 21)),
             ((0, 21), (319, 20)), ((10, 10), (10, 160)), ((10, 160), (10, 10))]
    for end in [(51, 31), (51, 32), (51, 50), (51, 51), (51, 90), (51, 100), (40, 120), (41, 120)]:
        pairs += [((30, 30), end), (end, (30, 30))]
    lines += ['struct line_sample {', '  graphics::pixel_position first;', '  graphics::pixel_position last;',
              '  std::uint64_t checksum;', '};', '', f'inline std::array<line_sample, {len(pairs)}> constexpr lines{{{{']
    for first, last in pairs:
        pixels = bytearray(320 * 240)
        for x, y, length in native_line([first, last]):
            pixels[y*320+x:y*320+x+length] = bytes([14]) * length
        lines.append(f'  {{.first{{.x{{{first[0]}}}, .y{{{first[1]}}}}}, .last{{.x{{{last[0]}}}, .y{{{last[1]}}}}}, .checksum{{0x{checksum(pixels):016x}ULL}}}},')
    lines += ['}};', '', 'struct endpoint_sample {', '  std::uint16_t pitch;', '  std::uint16_t roll;',
              '  std::int8_t pitch_high;', '  bool alternate;', '  graphics::attitude_line line;', '};', '',
              'inline std::array<endpoint_sample, 12> constexpr endpoints{{']
    for pitch, roll, high, mode in [(0, 0, -128, 0), (0, 0, -1, 0), (0, 0, 127, 1), (256, 0, 64, 0),
                                  (512, 0, 0, 0), (768, 0, -64, 1), (0, 256, 0, 0), (0, 512, 0, 0),
                                  (0, 768, 0, 0), (128, 1023, 1, 0), (1023, 128, -127, 1), (511, 511, 127, 1)]:
        value = Attitude().draw(pitch, roll, high, mode)
        first, last = value['points']
        lines.append(f'  {{.pitch{{{pitch}}}, .roll{{{roll}}}, .pitch_high{{{high}}}, .alternate{{{str(bool(mode)).lower()}}}, '
                     f'.line{{.first{{.x{{{first[0]}}}, .y{{{first[1]+8}}}}}, .last{{.x{{{last[0]}}}, .y{{{last[1]+8}}}}}, .colour{{{value["colour"]}}}}}}},')
    lines += ['}};', '']
    pixels = bytearray(320 * 240)
    for x, y, colour in data['attitude']['overlay']:
        pixels[(y+8)*320+x] = colour
    lines += [f'inline std::uint64_t constexpr attitude_surround_checksum{{0x{checksum(pixels):016x}ULL}};', '', '} // namespace darker::test_reference', '']
    args.output.write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
