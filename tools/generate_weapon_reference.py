#!/usr/bin/env python3
"""Execute original Skimma reload and ring producer paths to capture C++ test fixtures."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.workspace.resolve() / 'tools'))
    from procedural_hud import RAW, RingProducer
    from trace_hidden_commands import Harness
    assert hashlib.sha256(RAW).hexdigest() == '7599201a01aa24b7e6ad1ae4295d493821cde7c3a43b247c464522626c0380a4'
    reloads = []
    native = Harness(RAW)
    for weapon in range(3):
        for clock in (0, 65000):
            for working in (0, 1):
                for reserve in (0, 1, 5, 128, 129, 255):
                    native.setreg('BX', weapon)
                    native.write(0x5e10 + weapon, bytes([working]))
                    native.write(0x5e13 + weapon, bytes([reserve]))
                    native.write(0xbf8, struct.pack('<H', clock))
                    native.write(0xc51c, struct.pack('<H', 123))
                    native.write(0x5e00, struct.pack('<H', 252))
                    native.call(0x5e1f)
                    result = [native.read(0x5e10 + weapon)[0], native.read(0x5e13 + weapon)[0],
                              int.from_bytes(native.read(0xc51c, 2), 'little'), int.from_bytes(native.read(0x5e00, 2), 'little')]
                    reloads.append([weapon, clock, working, reserve, *result])
    rings = []
    native = RingProducer()
    for delta in (-32768, -1025, -1024, -960, -905, -904, -903, -512, -1, 0, 1, 32767):
        for enabled in (False, True):
            for spread in (0, 252, 508, 1024, 65535):
                calls = native.draw(delta, enabled, spread, 7)
                rings.append([delta, int(enabled), spread, int(bool(calls)), calls[0]['radius'] if calls else 0, calls[0]['count'] if calls else 0])
    class FullRing(RingProducer):
        def hook(self, cpu, address, size, data):
            if address - self.BASE == 0x5df9:
                Harness.hook(self, cpu, address, size, data)
                return
            super().hook(cpu, address, size, data)

    updates = []
    native = FullRing()
    for clock in (100, 65530):
        for delta in (-512, 0, 1):
            for spread in (0, 1, 252, 508, 32767, 32768, 65535):
                for step in (0, 1, 32, 65535):
                    for enabled in (0, 1):
                        for target in (0, 508, 65535):
                            native.calls = []
                            native.write(0xcf5b, b'\0\0')
                            for address, value in [(0xbf8, clock), (0xc51c, (clock-delta) & 65535), (0x5e00, spread), (0x7a2b, step), (0x5e03, target)]:
                                native.write(address, struct.pack('<H', value))
                            native.write(0x4543, bytes([enabled]))
                            native.write(0x5e10, b'\x07')
                            native.cpu.ctl_remove_cache(native.BASE+0x5dff, native.BASE+0x5e0f)
                            native.call(0x5d56)
                            calls = native.calls
                            updates.append([clock, delta, spread, step, enabled, int.from_bytes(native.read(0xc51c, 2), 'little'),
                                            int.from_bytes(native.read(0x5e00, 2), 'little'), int(bool(calls)), calls[0]['radius'] if calls else 0, calls[0]['count'] if calls else 0, target])

    class Status(Harness):
        def hook(self, cpu, address, size, data):
            if address - self.BASE == 0xc950:
                self.setreg('IP', self.RETURN)
                return
            super().hook(cpu, address, size, data)

    statuses = []
    native = Status(RAW)
    for slots in (2, 3):
        for selected in range(slots):
            for flags in (0, 1, 2, 3, 128, 129):
                for working, reserve in ((0, 0), (0, 2), (1, 0), (1, 3)):
                    for target in (-1, 0):
                        for count in (0, 1):
                            native.write(0xb91f, bytes([0x18 + slots]))
                            for address, value in [(0xcf5b, selected), (0xbf8, 65000), (0xc51c, 123), (0x5e00, 252), (0x5f0f, target & 65535), (0x6fbc, count)]:
                                native.write(address, struct.pack('<H', value))
                            native.write(0x4543, bytes([flags]*3))
                            native.write(0x5e10, bytes([working]*3))
                            native.write(0x5e13, bytes([reserve]*3))
                            native.call(0xc90a)
                            statuses.append([slots, selected, flags, working, reserve, target, count, native.read(0x454e)[0],
                                             *native.read(0x4543, 3), native.read(0x5e10+selected)[0], native.read(0x5e13+selected)[0],
                                             int.from_bytes(native.read(0xc51c, 2), 'little'), int.from_bytes(native.read(0x5e00, 2), 'little')])
    lines = ['#pragma once', '', '// Generated by tools/generate_weapon_reference.py using original machine code',
             '// Image SHA-256: ' + hashlib.sha256(RAW).hexdigest(), '', '#include <array>', '', 'namespace darker::test_reference {', '']
    for name, fields, rows in [('reload', 'weapon, clock, working, reserve, next_working, next_reserve, deadline, spread', reloads),
                                ('ring', 'delta, enabled, spread, visible, radius, remaining', rings),
                                ('update', 'clock, delta, spread, step, enabled, deadline, next_spread, visible, radius, remaining, target', updates),
                                ('status', 'slots, selected, flags, working, reserve, target, count, display, flag0, flag1, flag2, next_working, next_reserve, deadline, spread', statuses)]:
        lines += [f'struct {name}_sample {{ int {fields}; }};', f'inline constexpr std::array<{name}_sample, {len(rows)}> {name}_samples{{{{']
        lines += ['  {' + ', '.join(map(str, row)) + '},' for row in rows]
        lines += ['}};', '']
    lines += ['} // namespace darker::test_reference', '']
    (Path(__file__).resolve().parents[1] / 'tests/reference/weapon_samples.h').write_text('\n'.join(lines))
    print(f'Captured {len(reloads)} reloads, {len(rings)} ring decisions, {len(updates)} full ring updates and {len(statuses)} status updates.')


if __name__ == '__main__':
    main()
