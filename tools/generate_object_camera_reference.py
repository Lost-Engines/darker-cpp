#!/usr/bin/env python3
"""Capture native object attitude composition into camera projection axes."""
import argparse
from pathlib import Path
import random
import struct
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('workspace',type=Path);args=parser.parse_args()
    sys.path.insert(0,str(args.workspace.resolve()/'tools'))
    from trace_hidden_commands import Harness
    image=(args.workspace/'analysis/unpacked/image.bin').read_bytes();h=Harness(image);rng=random.Random(0x1e9e);cases=[]
    h.write(0x9c4a,image[0x944a:0x964a]);h.write(0xfdec,struct.pack('<H',0x1000))
    for i in range(512):
        camera=[rng.randrange(65536) for _ in range(3)];obj=[rng.randrange(65536) for _ in range(3)]
        for reg,angle in zip(('DI','BX','CX'),camera):h.setreg(reg,(((angle+15)&65535)>>6)*2)
        h.call(0x1d63)
        h.setreg('BP',0x7000);h.cpu.mem_write(h.STACK+0x701c,struct.pack('<3H',*obj));h.call(0x1e9e)
        values=[struct.unpack('<h',h.read(at,2))[0] for at in (0xfd05,0xfd12,0xfd1f,0xfd31,0xfd3e,0xfd4b,0xfd5d,0xfd6a,0xfd77)]
        cases.append((camera,obj,values))
    lines=['#pragma once','','// Generated from native 1D63 and 1E9E','#include <array>','','namespace darker::test_reference {',
           'struct object_camera_sample { std::array<int, 3> camera, object; std::array<int, 9> axes; };',
           f'inline constexpr std::array<object_camera_sample, {len(cases)}> object_camera_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,x))+'}' for x in row)+'},' for row in cases]
    lines+=['}};','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/object_camera_samples.h').write_text('\n'.join(lines))


if __name__=='__main__':main()
