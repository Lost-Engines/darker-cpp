#!/usr/bin/env python3
"""Capture native low-altitude and shield display producers, including deadline mutations."""
import argparse
from pathlib import Path
import random
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.workspace.resolve() / 'tools'))
    from trace_hidden_commands import Harness
    image = (args.workspace / 'analysis/unpacked/image.bin').read_bytes()
    h = Harness(image)
    h.setreg('BP', 0xd986)
    rng = random.Random(0x57b7)
    samples = []
    for i in range(1024):
        height = rng.choice([0, 1023, 1024, 65535, rng.randrange(65536)])
        charge = rng.randrange(0xc000)
        enabled, flash = rng.randrange(2), rng.randrange(2)
        clock = rng.randrange(65536)
        delta = rng.choice([0, 1, 255, 256, 511, 512, 767, 768, 1791, 65535, rng.randrange(2048)])
        deadline = (clock + delta) & 65535
        for at, value in ((0x57b8, deadline), (0x8109, charge), (0xbf8, clock)):
            h.write(at, struct.pack('<H', value))
        h.cpu.mem_write(h.STACK+0xd992, struct.pack('<H', height))
        h.write(0x4552, bytes([enabled]))
        h.write(0x7e61, bytes([flash]))
        h.write(0x38fa+6, bytes([0x12]))
        h.write(0x4546, bytes([0]))
        h.cpu.ctl_remove_cache(h.BASE, h.BASE+65536)
        h.cpu.emu_start(h.BASE+0x579c, h.BASE+0x582b, count=1000)
        assert h.getreg('IP') == 0x582b
        output = [h.read(0x454b)[0], h.read(0x454c)[0], h.read(0x4546)[0], int.from_bytes(h.read(0x57b8, 2), 'little')]
        h.setreg('AX', output[2])
        h.call(0x5845)
        output += [h.getreg('BX') & 255, h.getreg('AX') & 255]
        samples.append(([height, charge, enabled, flash, clock, deadline], output))
    lines = ['#pragma once', '', '// Generated from native 579C–5828; sound writes excluded', '#include <array>', '', 'namespace darker::test_reference {',
             'struct skimma_instruments_sample { std::array<int, 6> input; std::array<int, 6> output; };',
             f'inline constexpr std::array<skimma_instruments_sample, {len(samples)}> skimma_instruments_samples{{{{']
    lines += ['  {'+', '.join('{'+', '.join(map(str, p))+'}' for p in row)+'},' for row in samples]
    lines += ['}};', '} // namespace darker::test_reference', '']
    (Path(__file__).resolve().parents[1]/'tests/reference/skimma_instruments_samples.h').write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
