#!/usr/bin/env python3
"""Capture original attached missile cameras for live shots and their impact effect phase."""
import argparse
from pathlib import Path
import random
import struct
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('workspace',type=Path);args=parser.parse_args()
    sys.path.insert(0,str(args.workspace.resolve()/'tools'))
    from trace_hidden_commands import Harness
    image=(args.workspace/'analysis/unpacked/image.bin').read_bytes();h=Harness(image);rng=random.Random(0x2464);cases=[]
    h.write(0x9c4a,image[0x944a:0x964a]);h.write(0x2655,b'\0\0');h.write(0x2659,b'\0\0')
    for i in range(1024):
        position=[rng.randrange(65536) for _ in range(2)]+[rng.randrange(4096)]
        fractions=[rng.randrange(256) for _ in range(2)]
        angles=[rng.randrange(65536) for _ in range(3)]
        mode=rng.randrange(4);step=rng.randrange(1,81);setting=rng.randrange(6);distance=rng.choice([32768,rng.randrange(9000)])
        landed=rng.randrange(2);hidden=rng.randrange(2)
        for offset,data in [(8,struct.pack('<3H',*position)),(4,bytes(fractions)),(0x2a,struct.pack('<3H',*angles)),(7,bytes([8 if hidden else 0]))]:
            h.cpu.mem_write(h.STACK+0xda00+offset,data)
        h.cpu.mem_write(h.STACK+0xd98d,bytes([16 if landed else 0]))
        for at,value in [(0x7a2b,step),(0x255d,distance),(0x78f2,0xda00)]:h.write(at,struct.pack('<H',value))
        h.write(0x4540,bytes([mode+7]));h.write(0xb8b8,bytes([setting]));h.write(0xf003,b'\0')
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536);h.call(0x2409)
        val=lambda at:int.from_bytes(h.read(at,2),'little')
        result=[val(0x2700),val(0x271b),val(0x2e36),*h.read(0x270c,2),val(0x3e46)*32,val(0x3e43)*32,val(0x3e49)*32,val(0x255d)]
        cases.append((position,fractions,angles,[mode,step,setting,distance,landed,hidden],result))
    lines=['#pragma once','','// Generated from native 2409 for attached missile cameras','#include <array>','','namespace darker::test_reference {',
           'struct missile_camera_sample { std::array<int,3> position; std::array<int,2> fractions; std::array<int,3> angles; std::array<int,6> input; std::array<int,9> output; };',
           f'inline constexpr std::array<missile_camera_sample,{len(cases)}> missile_camera_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,x))+'}' for x in row)+'},' for row in cases]
    lines+=['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/missile_camera_samples.h').write_text('\n'.join(lines))
    print('Captured',len(cases),'native missile camera frames.')


if __name__=='__main__':main()
