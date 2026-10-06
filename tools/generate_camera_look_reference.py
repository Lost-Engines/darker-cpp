#!/usr/bin/env python3
"""Capture native Tab look offsets and released-view return smoothing."""
import argparse
from pathlib import Path
import random
import struct
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('workspace',type=Path);args=parser.parse_args()
    sys.path.insert(0,str(args.workspace.resolve()/'tools'))
    from trace_hidden_commands import Harness
    image=(args.workspace/'analysis/unpacked/image.bin').read_bytes();h=Harness(image);rng=random.Random(0x7872);cases=[]
    for i in range(1024):
        heading=rng.randrange(65536);pitch=rng.randrange(65536);bank=rng.randrange(65536);drive=rng.randrange(65536)
        held=rng.randrange(2);step=rng.randrange(1,81);mode=rng.randrange(4);landed=rng.randrange(2)
        for at,value in [(0x2655,heading),(0x2659,pitch),(0x7eba,bank),(0x7ecf,drive),(0x7a2b,step),(0xb9f4,256 if held else 0)]:h.write(at,struct.pack('<H',value))
        h.write(0x78e7,bytes([mode]));h.write(0x4540,bytes([mode]));h.cpu.mem_write(h.STACK+0xd98d,bytes([16 if landed else 0]))
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
        h.cpu.emu_start(h.BASE+0x7872,h.BASE+0x78e6,count=1000)
        assert h.getreg('IP')==0x78e6
        output=[int.from_bytes(h.read(at,2),'little') for at in [0x2655,0x2659]]
        cases.append(([heading,pitch,bank,drive,held,step,mode,landed],output))
    lines=['#pragma once','','// Generated from native 7872–78E6 and 7CA4','#include <array>','','namespace darker::test_reference {',
           'struct camera_look_sample { std::array<int, 8> input; std::array<int, 2> output; };',f'inline constexpr std::array<camera_look_sample, {len(cases)}> camera_look_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,x))+'}' for x in row)+'},' for row in cases]
    lines+=['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/camera_look_samples.h').write_text('\n'.join(lines))


if __name__=='__main__':main()
