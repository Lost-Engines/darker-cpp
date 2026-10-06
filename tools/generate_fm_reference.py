#!/usr/bin/env python3
"""Export original FM patches and capture sequenced nonspatial OPL voice programming."""
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
    sys.path.insert(0, str(args.workspace.resolve()/'tools'))
    from extract_sound_effects import Probe
    image = (args.workspace/'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256
    root = Path(__file__).resolve().parents[1]
    lines = ['#pragma once', '', '// Generated from native 0F26; image SHA-256: '+IMAGE_SHA256,
             '#include <array>', '#include <cstdint>', '', 'namespace darker::audio {',
             'inline constexpr std::array<std::array<std::uint8_t, 10>, 39> fm_patches{{']
    lines += ['  {'+', '.join(str(x) for x in image[0xf26+i*10:0xf30+i*10])+'},' for i in range(39)]
    lines += ['}};', '} // namespace darker::audio', '']
    (root/'src/audio/fm_patches.h').write_text('\n'.join(lines))
    h = Probe(); rng = random.Random(0x39a9); cases = []
    for channel, operator in enumerate([0, 1, 2, 8, 9, 10, 16, 17, 18]):
        h.write(0x10ac+channel*8, bytes([0, 0, channel, operator, 0, 255, 31, 0]))
    for i in range(1024):
        channel = rng.randrange(9); patch = rng.randrange(39); pitch = rng.randrange(65536); level = rng.randrange(65536)
        key = rng.randrange(2); retrigger = rng.randrange(2) if key else 0
        record = bytearray(20)
        struct.pack_into('<H', record, 4, pitch);struct.pack_into('<H', record, 14, level)
        record[7] = 4 | (0 if retrigger else 8) | (32 if key else 0)
        record[19] = patch
        h.cpu.mem_write(h.STACK+0x7000, bytes(record));h.setreg('BP', 0x7000);h.setreg('DI', 0x10ac+channel*8)
        h.events = [];h.call(0x39a9)
        writes = [r*256+v for _,r,v in h.events]
        assert len(writes) <= 16
        cases.append(([channel, patch, pitch, level, key, retrigger], len(writes), writes+[0]*(16-len(writes))))
    lines = ['#pragma once', '', '// Generated from native 1121/10F4/39A9, with native voice caches retained', '#include <array>', '',
             'namespace darker::test_reference {', 'struct fm_sample { std::array<int, 6> input; unsigned int count; std::array<int, 16> writes; };',
             f'inline constexpr std::array<fm_sample, {len(cases)}> fm_samples{{{{']
    lines += ['  {{'+', '.join(map(str,x))+'}, '+str(n)+', {'+', '.join(map(str,w))+'}},' for x,n,w in cases]
    lines += ['}};', '} // namespace darker::test_reference', '']
    (root/'tests/reference/fm_samples.h').write_text('\n'.join(lines))
    engines = []
    for i in range(512):
        skimma = rng.randrange(2); upgraded = rng.randrange(2); speed = rng.randrange(65536)
        clock = rng.randrange(65536); enabled = rng.randrange(2); crashed = rng.randrange(2)
        slot = 25 if not skimma else 27 if upgraded else 26
        base_pitch = int.from_bytes(image[0x1926+slot*24+22:0x1926+slot*24+24], 'little')
        h.setreg('BP', 0xd986);h.setreg('DI', base_pitch);h.setreg('SI', 0xc4ff if not skimma else 0xe4ff)
        h.cpu.mem_write(h.STACK+0xd98d, bytes([8 | (32 if crashed else 0)]))
        h.cpu.mem_write(h.STACK+0xd9c4, struct.pack('<H', speed))
        h.cpu.mem_write(h.STACK+0xd9eb, bytes([0]))
        h.write(0xbf0, struct.pack('<H', clock));h.write(0x4552, bytes([enabled]));h.write(0xf003, bytes([2 if skimma else 0]))
        h.call(0x3914 if skimma else 0x3980)
        active = int(bool(h.getreg('EFLAGS') & 64))
        engines.append(([skimma, upgraded, speed, clock, enabled, crashed], [h.getreg('DI'), h.getreg('SI'), active]))
    lines = ['#pragma once', '', '// Generated from native 3980/3914 with the cockpit-hidden player record', '#include <array>', '',
             'namespace darker::test_reference {', 'struct engine_sound_sample { std::array<int, 6> input; std::array<int, 3> output; };',
             f'inline constexpr std::array<engine_sound_sample, {len(engines)}> engine_sound_samples{{{{']
    lines += ['  {'+', '.join('{'+', '.join(map(str, p))+'}' for p in row)+'},' for row in engines]
    lines += ['}};', '} // namespace darker::test_reference', '']
    (root/'tests/reference/engine_sound_samples.h').write_text('\n'.join(lines))



if __name__ == '__main__':
    main()
