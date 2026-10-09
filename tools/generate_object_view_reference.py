#!/usr/bin/env python3
"""Capture native F7 ray construction, range checks and attached object views."""
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
    image = (args.workspace / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256
    h = Harness(image)
    h.write(0x9c4a, image[0x944a:0x964a])
    word = lambda x: struct.pack('<H', x & 65535)
    read = lambda a: int.from_bytes(h.read(a, 2), 'little')
    rays = []
    views = []
    rng = random.Random(0x257d)

    def stop_ray(cpu, address, size, data):
        if address == h.BASE + 0x25b9:
            h.setreg('IP', 0x254c)  # RET before collision; test its input separately.
    h.cpu.hook_add(UC_HOOK_CODE, stop_ray)
    for n in range(512):
        position = [rng.randrange(65536) for _ in range(3)]
        angles = [rng.randrange(65536) for _ in range(3)]
        target = [rng.randrange(65536) for _ in range(3)]
        for reg, value in zip(('DI','BX','CX'), angles):
            h.setreg(reg, (((value + 15) & 65535) >> 6)*2)
        h.cpu.ctl_remove_cache(h.BASE, h.BASE + 65536)
        h.call(0x1d63)
        h.write(0x2e36, word(position[2]))
        h.setreg('DX', position[0]); h.setreg('BX', position[1])
        h.call(0x2591)
        end = [h.getreg('CX'), h.getreg('DX'), (h.getreg('BP')*2) & 65535]
        h.cpu.mem_write(h.STACK + 0xd98e, struct.pack('<3H', *position))
        h.setreg('BP', 0xd986); h.setreg('DX', target[0]); h.setreg('BX', target[1]); h.call(0x841c)
        rays.append((position.copy(), angles, target, end, [int(h.getreg('AX') < 4096)]))
        position[2] &= 4095
        fractions = [rng.randrange(256) for _ in range(2)]
        step, setting, distance, dead = rng.randrange(1,81), rng.randrange(6), rng.choice([32768,rng.randrange(9000)]), n % 2
        for off, data in [(8,struct.pack('<3H',*position)),(4,bytes(fractions)),(0x2a,struct.pack('<3H',*angles)),(7,bytes([8 if dead else 0]))]:
            h.cpu.mem_write(h.STACK + 0xdb00 + off, data)
        for at, value in [(0x2449,0xdb00),(0x7a2b,step),(0x255d,distance),(0x2655,0),(0x2659,0)]: h.write(at,word(value))
        h.write(0x4540,b'\x06'); h.write(0xb8b8,bytes([setting])); h.write(0xf003,b'\0')
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536);h.call(0x2409)
        result = [read(0x2700),read(0x271b),read(0x2e36),*h.read(0x270c,2),read(0x3e46)*32,read(0x3e43)*32,read(0x3e49)*32,read(0x255d)]
        views.append((position,fractions,angles,[step,setting,distance,dead],result))
    lines = ['#pragma once','','// Generated native 2591/841C and 2409 mode 6; collision selection is not substituted into these cases.',
             '// Image SHA-256: '+IMAGE_SHA256,'#include <array>','','namespace darker::test_reference {',
             'struct object_camera_ray_sample { std::array<int,3> position, angles, target, end; std::array<int,1> accepted; };',
             'inline constexpr std::array<object_camera_ray_sample,512> object_camera_rays{{']
    encode = lambda rows: ['  {'+', '.join('{'+', '.join(map(str,v))+'}' for v in row)+'},' for row in rows]
    lines += encode(rays)+['}};','struct object_camera_view_sample { std::array<int,3> position; std::array<int,2> fractions; std::array<int,3> angles; std::array<int,4> input; std::array<int,9> output; };',
                            'inline constexpr std::array<object_camera_view_sample,512> object_camera_views{{']+encode(views)+['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/object_view_samples.h').write_text('\n'.join(lines))
    print('Captured 512 F7 rays/ranges and 512 object camera views')


if __name__ == '__main__':
    main()
