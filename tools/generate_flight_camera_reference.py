#!/usr/bin/env python3
"""Capture original cockpit, full-screen and following player cameras."""
import argparse
from pathlib import Path
import random
import struct
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('workspace',type=Path);args=parser.parse_args()
    sys.path.insert(0,str(args.workspace.resolve()/'tools'))
    from trace_hidden_commands import Harness
    image=(args.workspace/'analysis/unpacked/image.bin').read_bytes();h=Harness(image);rng=random.Random(0x254d);cases=[]
    h.write(0x9c4a,image[0x944a:0x964a]);h.write(0x2655,b'\0\0');h.write(0x2659,b'\0\0')
    for i in range(512):
        position=[rng.randrange(65536) for _ in range(2)]+[rng.randrange(4096)]
        fractions=[rng.randrange(256) for _ in range(2)]
        angles=[rng.randrange(65536) for _ in range(3)]
        mode=rng.randrange(4);step=rng.randrange(1,81);setting=rng.randrange(6);distance=rng.choice([32768,rng.randrange(9000)]);landed=rng.randrange(2)
        for offset,data in [(8,struct.pack('<3H',*position)),(4,bytes(fractions)),(0x2a,struct.pack('<3H',*angles)),(7,bytes([16 if landed else 0]))]:
            h.cpu.mem_write(h.STACK+0xd986+offset,data)
        for at,value in [(0x7a2b,step),(0x255d,distance)]:h.write(at,struct.pack('<H',value))
        h.write(0x4540,bytes([mode]));h.write(0xb8b8,bytes([setting]));h.write(0xf003,b'\0')
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536);h.call(0x2409)
        val=lambda at:int.from_bytes(h.read(at,2),'little')
        result=[val(0x2700),val(0x271b),val(0x2e36),*h.read(0x270c,2),val(0x3e46)*32,val(0x3e43)*32,val(0x3e49)*32,val(0x255d)]
        cases.append((position,fractions,angles,[mode,step,setting,distance,landed],result))
    lines=['#pragma once','','// Generated from native 2409 with ordinary player view offsets zero','#include <array>','','namespace darker::test_reference {',
           'struct flight_camera_sample { std::array<int, 3> position; std::array<int, 2> fractions; std::array<int, 3> angles; std::array<int, 5> input; std::array<int, 9> output; };',
           f'inline constexpr std::array<flight_camera_sample, {len(cases)}> flight_camera_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,x))+'}' for x in row)+'},' for row in cases]
    lines+=['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/flight_camera_samples.h').write_text('\n'.join(lines))
    cases=[]
    for i in range(256):
        anchor=[rng.randrange(32768),rng.randrange(32768),rng.randrange(4096)]
        angles=[rng.randrange(65536) for _ in range(3)]
        player=[(v+rng.randrange(-500,501))&65535 for v in anchor]
        mode=rng.choice([4,5])
        h.cpu.mem_write(h.STACK+0xd98e,struct.pack('<3H',*anchor));h.cpu.mem_write(h.STACK+0xd9b0,struct.pack('<3H',*angles))
        h.call(0x260d)
        h.cpu.mem_write(h.STACK+0xd98e,struct.pack('<3H',*player));h.write(0x4540,bytes([mode]))
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536);h.call(0x2409)
        result=[val(0x2700),val(0x271b),val(0x2e36),*h.read(0x270c,2),val(0x3e46)*32,val(0x3e43)*32,val(0x3e49)*32]
        cases.append((anchor,angles,player,[mode],result))
    lines=['#pragma once','','// Generated from native 260D/2409 for dropped player cameras','#include <array>','','namespace darker::test_reference {',
           'struct dropped_camera_sample { std::array<int, 3> anchor, angles, player; std::array<int, 1> mode; std::array<int, 8> output; };',
           f'inline constexpr std::array<dropped_camera_sample, {len(cases)}> dropped_camera_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,x))+'}' for x in row)+'},' for row in cases]
    lines+=['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/dropped_camera_samples.h').write_text('\n'.join(lines))



if __name__=='__main__':main()
