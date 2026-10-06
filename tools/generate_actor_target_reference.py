#!/usr/bin/env python3
"""Capture actor target selection and object/city direction resolution."""
import argparse
import hashlib
from pathlib import Path
import random
import struct
import sys
from generate_resource_directory import IMAGE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.workspace.resolve() / 'tools'))
    from trace_hidden_commands import Harness
    from unicorn import UC_HOOK_CODE
    raw = (args.workspace / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == IMAGE_SHA256
    h = Harness(raw)
    rng = random.Random(0x8826)
    mode = 'select'
    captured = []
    nearby_height = 0

    def hook(cpu, address, size, data):
        if mode == 'city_scan' and address == h.BASE + 0x8962:
            captured[:] = [h.getreg('DX')]
            h.setreg('IP', h.RETURN)
        elif mode == 'clearance' and address == h.BASE + 0x8928:
            # Skip neighbour enumeration; supply its separately resolved maximum height.
            h.setreg('DX', nearby_height)
            h.setreg('IP', 0x8966)
        elif mode == 'clearance' and address == h.BASE + 0x89a2:
            captured[:] = [int.from_bytes(h.read(0x88ad, 2), 'little'), h.read(0x89ae)[0],
                           int.from_bytes(cpu.mem_read(h.STACK + 0xda4e, 2), 'little')]
            h.setreg('IP', h.RETURN)
        elif mode == 'select' and address == h.BASE + 0x883f:
            captured[:] = [int.from_bytes(cpu.mem_read(h.STACK + 0xda48, 2), 'little')]
            h.setreg('IP', h.RETURN)
        elif mode == 'course' and address == h.BASE + 0x88e4:
            captured[:] = [h.getreg('DI'), h.getreg('AX'), h.getreg('SI')]
            h.setreg('IP', h.RETURN)

    h.cpu.hook_add(UC_HOOK_CODE, hook)
    groups = []
    rows = []
    for i in range(512):
        level = rng.choice([0, 1, 255, 256, 65535, rng.randrange(65536)])
        threshold = rng.randrange(256)
        selected = rng.choice([0, 0xd986, 0xdb00, rng.randrange(32768)])
        target = rng.randrange(65536)
        obj = bytearray(112)
        struct.pack_into('<HH', obj, 0x48, selected, target)
        obj[0x51] = threshold
        h.cpu.mem_write(h.STACK + 0xda00, bytes(obj))
        h.setreg('BP', 0xda00)
        h.setreg('BX', level)
        h.call(0x8826)
        rows.append(([level, threshold, selected, target], captured.copy()))
    groups.append(('actor_target', rows))
    mode = 'course'
    rows = []
    for i in range(512):
        position = [rng.randrange(65536) for _ in range(3)]
        target = [rng.randrange(65536) for _ in range(3)]
        if i % 2:
            target[:2] = [(n + rng.randrange(-256, 256)) & 65535 for n in position[:2]]
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, *position)
        h.cpu.mem_write(h.STACK + 0xda00, bytes(obj))
        struct.pack_into('<3H', obj, 8, *target)
        h.cpu.mem_write(h.STACK + 0xdb00, bytes(obj))
        h.setreg('BP', 0xda00)
        h.setreg('BX', 0xdb00)
        h.call(0x88b9)
        rows.append((position + target, captured.copy()))
    groups.append(('actor_object_course', rows))
    rows = []
    h.write(0xfdf0, struct.pack('<H', 0x3000))
    h.write(0xfdec, struct.pack('<H', 0x1000))
    h.write(0x3142, struct.pack('<H', 0x7000))
    h.cpu.mem_write(0x30000, bytes([1, 0]) * 16384)
    h.cpu.mem_write(0x3a000, struct.pack('<H', 0x1000))
    for i in range(512):
        position = [rng.randrange(65536) for _ in range(3)]
        cell = rng.randrange(32768)
        slot = rng.choice([19, 22, 23])
        speed = rng.randrange(65536)
        nominal, fx, fy, marker = [rng.randrange(256) for _ in range(4)]
        height = rng.randrange(-32768, 32768)
        extent = rng.randrange(65536)
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, *position)
        obj[0x21] = slot
        struct.pack_into('<H', obj, 0x3e, speed)
        struct.pack_into('<H', obj, 0x44, 0xf000)
        struct.pack_into('<H', obj, 0x48, cell)
        h.cpu.mem_write(h.STACK + 0xda00, bytes(obj))
        h.cpu.mem_write(0x3a008, struct.pack('<HBBBBBB', 0x400, fx, fy, marker, 0, 0, 0))
        h.cpu.mem_write(0x70407, struct.pack('<hH', height, extent))
        h.write(0xf00c, bytes([nominal]))
        h.setreg('BP', 0xda00)
        h.setreg('DS', 0x1000)
        h.setreg('AX', cell)
        h.call(0x883f)
        rows.append(([*position, cell, slot, speed, nominal, fx, fy, marker, height, extent], captured.copy()))
    groups.append(('actor_cell_course', rows))
    rows = []
    mode = 'clearance'
    for i in range(1024):
        position = [rng.randrange(65536) for _ in range(3)]
        reference = rng.randrange(65536) if i % 2 else (position[0] >> 8) | (position[1] & 0xff00)
        floor, nearby_height, pitch = [rng.randrange(65536) for _ in range(3)]
        climb = rng.randrange(256)
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, *position)
        struct.pack_into('<HH', obj, 0x4c, reference, floor)
        h.cpu.mem_write(h.STACK + 0xda00, bytes(obj))
        h.write(0x88ad, struct.pack('<H', pitch))
        h.write(0x89ae, bytes([climb]))
        h.cpu.ctl_remove_cache(h.BASE, h.BASE + 65536)
        h.setreg('BP', 0xda00)
        h.setreg('DS', 0x1000)
        h.call(0x8908)
        rows.append(([*position, reference, floor, nearby_height, pitch, climb], captured.copy()))
    groups.append(('actor_clearance', rows))
    rows = []
    mode = 'neighbour'
    for i in range(1024):
        position = [rng.randrange(65536) for _ in range(3)]
        target = [(position[axis] + rng.randrange(-1400, 1400)) & 65535 for axis in range(3)]
        if i % 4 == 0:
            target[2] = position[2]
        speed, pitch = rng.randrange(65536), rng.randrange(65536)
        floor = rng.choice([0, 1900, rng.randrange(65536)])
        reference = rng.randrange(65536)
        climb, base = rng.randrange(256), rng.randrange(256)
        reverse = rng.randrange(2)
        current, other = (0xdb00, 0xda00) if reverse else (0xda00, 0xdb00)
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, *position)
        struct.pack_into('<H', obj, 0x44, 0xf000)
        struct.pack_into('<HH', obj, 0x4c, reference, floor)
        h.cpu.mem_write(h.STACK + current, bytes(obj))
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, *target)
        struct.pack_into('<H', obj, 0x2c, pitch)
        struct.pack_into('<H', obj, 0x3e, speed)
        h.cpu.mem_write(h.STACK + other, bytes(obj))
        h.write(0x6fd8, struct.pack('<H', other))
        h.write(0xf00e, bytes([base]))
        h.write(0x89ae, bytes([climb]))
        h.cpu.ctl_remove_cache(h.BASE, h.BASE + 65536)
        h.setreg('BP', current)
        h.setreg('DI', 0x6fd8)
        h.setreg('SI', 1023)
        h.setreg('AX', 0x1e5f)
        h.setreg('DS', 0x1000)
        h.call(0x6db5)
        state = bytes(h.cpu.mem_read(h.STACK + current, 112))
        rows.append(([*position, *target, speed, pitch, floor, reference, climb, base, reverse],
                     [int.from_bytes(state[0x4e:0x50], 'little'), int.from_bytes(state[0x4c:0x4e], 'little'), h.read(0x89ae)[0]]))
    groups.append(('actor_neighbour', rows))
    rows = []
    mode = 'city_scan'
    for i in range(256):
        column, row = [rng.choice([0, 1, 126, 127, 128, 255, rng.randrange(128)]) for _ in range(2)]
        mask = rng.choice([32, 96])
        marker = rng.randrange(256)
        heights = [rng.randrange(-32768, 32768) for _ in range(6)]
        extents = [rng.randrange(65536) for _ in range(6)]
        neighbourhood = [value for _ in range(9) for value in [rng.randrange(2), rng.randrange(256)]]
        obj = bytearray(112)
        struct.pack_into('<2H', obj, 8, column * 256 + 128, row * 256 + 128)
        h.cpu.mem_write(h.STACK + 0xda00, bytes(obj))
        cells = bytearray(32768)
        for n in range(9):
            x, y = (column + n % 3 - 1) & 255, (row + n // 3 - 1) & 255
            if x < 128 and y < 128:
                cells[y * 256 + x * 2:y * 256 + x * 2 + 2] = bytes(neighbourhood[n * 2:n * 2 + 2])
        h.cpu.mem_write(0x30000, bytes(cells))
        h.cpu.mem_write(0x3a008, struct.pack('<HBBBBBB', 0x400, 128, 128, marker, 0, 3, 0))
        models = bytearray(96)
        for n in range(6):
            struct.pack_into('<hh', models, n * 16, (((n + 1) % 6) - n) * 16, (((n + 2) % 6) - n) * 16)
            struct.pack_into('<hH', models, n * 16 + 7, heights[n], extents[n])
        h.cpu.mem_write(0x70400, bytes(models))
        h.write(0x2e10, bytes([mask * 2]))
        h.cpu.ctl_remove_cache(h.BASE, h.BASE + 65536)
        h.setreg('BP', 0xda00)
        h.setreg('DS', 0x1000)
        h.call(0x8935)
        rows.append(([column, row, mask, marker, *heights, *extents, *neighbourhood], captured.copy()))
    groups.append(('actor_city_scan', rows))
    lines = ['#pragma once', '', '// Generated by tools/generate_actor_target_reference.py; native 8826/883F/88B9',
             '// Image SHA-256: ' + IMAGE_SHA256, '', '#include <array>', '', 'namespace darker::test_reference {', '']
    for name, rows in groups:
        lines += [f'struct {name}_sample {{ std::array<int, {len(rows[0][0])}> input; std::array<int, {len(rows[0][1])}> output; }};',
                  f'inline constexpr std::array<{name}_sample, {len(rows)}> {name}_samples{{{{']
        lines += ['  {{' + ', '.join(map(str, a)) + '}, {' + ', '.join(map(str, b)) + '}},' for a, b in rows]
        lines += ['}};', '']
    lines += ['} // namespace darker::test_reference', '']
    (Path(__file__).resolve().parents[1] / 'tests/reference/actor_target_samples.h').write_text('\n'.join(lines))
    print('Captured native target, course, clearance and neighbour cases, including 256 eight-cell city scans.')


if __name__ == '__main__':
    main()
