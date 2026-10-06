#!/usr/bin/env python3
"""Fingerprint native BB90/BBC6 on both original persistent city maps."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'tools'))
from inspect_city import geometry_bank
from inspect_persistent_map_state import State


def fingerprint(data):
    result = 0xcbf29ce484222325
    for byte in data:
        result = ((result ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return result


rows = []
for bank, map_id in ((30, 68), (31, 69)):
    raw = (ROOT.parent / f'analysis/resources/00_{bank:03}.bin').read_bytes()
    types = {t['type']: bytes.fromhex(t['record_hex']) for t in geometry_bank(raw, bank)['types']}
    city = (ROOT.parent / f'analysis/resources/00_{map_id:03}.bin').read_bytes()
    eligible = [i for i, t in enumerate(city) if t and types[t][4] != 255]
    h = State()
    h.word(0xfdf0, 0x6000)
    h.cpu.mem_write(0x6a004, b'\xff')
    for t, record in types.items():
        h.cpu.mem_write(0x6a000 + t * 8, record)
    runtime = bytearray(32768)
    runtime[::2] = city
    for j, i in enumerate(eligible):
        runtime[2*i+1] = ((j % 4) << 5) | ((j*7) & 31) | ((j & 1) << 7)
    h.cpu.mem_write(0x60000, bytes(runtime))
    h.setreg('ES', 0x7000)
    h.setreg('DI', 0x100)
    h.call(0xbb90)
    size = h.getreg('DI') - 0x100
    packed = bytes(h.cpu.mem_read(0x70100, size))
    template = bytearray(32768)
    template[::2] = city
    if bank == 30:
        for i, t in enumerate(city):
            if t == 1 and i % 128 % 9 == 0 and i // 128 % 9 == 0:
                template[2*i+1] = 255
    for stage in (1, 2):
        h.cpu.mem_write(0x60000, bytes(template))
        h.cpu.mem_write(0x70000, bytes([stage]))
        h.setreg('ES', 0x7000)
        h.setreg('SI', 0)
        h.setreg('DI', 0x100)
        h.call(0xbbc6)
        states = bytes(h.cpu.mem_read(0x60000, 32768))[1::2]
        rows.append(f'  {{{bank}, {map_id}, {stage}, {size}, 0x{fingerprint(packed):016x}ULL, 0x{fingerprint(states):016x}ULL}},')
(ROOT / 'tests/reference/city_persistence_samples.h').write_text('''#pragma once
#include <array>
#include <cstdint>

namespace darker::test_reference {
struct city_persistence_sample { unsigned int bank, map, stage, bytes; uint64_t packed, restored; };
inline constexpr auto city_persistence_samples = std::to_array<city_persistence_sample>({
''' + '\n'.join(rows) + '\n});\n} // namespace darker::test_reference\n')
print('Native city packing and restored-state fingerprints generated for both cities and stages 1/2.')
