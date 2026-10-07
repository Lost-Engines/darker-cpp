#!/usr/bin/env python3
"""Capture native aircraft extent sweeps, falling motion and close-range gun shots."""
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
    raw = (args.workspace / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == IMAGE_SHA256
    h = Harness(raw)
    h.write(0x9c4a, raw[0x944a:0x964a])
    rng = random.Random(0x8daa)
    sweeps, falling, guns = [], [], []
    for i in range(512):
        position = [rng.randrange(1000, 30000), rng.randrange(1000, 30000), rng.randrange(500, 10000)]
        extent = rng.randrange(20, 800)
        expansion = rng.choice((10, 12))
        start = [(v + rng.randrange(-300, 300)) & 65535 for v in position]
        end = [(v + rng.randrange(-300, 300)) & 65535 for v in position]
        fields = ((0x6469,0x6476),(0x64b0,0x64a2),(0x6493,0x6486))
        flags = 0
        for axis, (a,b) in enumerate(fields):
            low, high = start[axis], end[axis]
            if axis == 2: low >>= 3; high >>= 3
            h.write(a, struct.pack('<H', min(low, high))); h.write(b, struct.pack('<H', max(low, high)))
            if high >= low: flags |= (4,1,2)[axis]
        h.write(0x611a, bytes([flags]))
        for reg, val in [('CX',position[0]),('BP',position[0]),('DX',position[1]),('BX',position[1]),('SI',position[2]>>3),('DI',position[2]>>3),('AX',(extent>>2)+expansion)]: h.setreg(reg,val)
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
        h.call(0x60c8)
        hit = int(not(h.getreg('EFLAGS') & 1))
        result = end.copy()
        if hit:
            h.call(0x662b); h.call(0x6705)
            result = [h.getreg('DX'),h.getreg('BX'),h.getreg('CX')]
        sweeps.append([*position,extent,expansion,*start,*end,hit,*result])
        angles = [rng.randrange(65536) for _ in range(3)]
        rates = [rng.randrange(65536) for _ in range(2)]
        speed = rng.randrange(1000)
        step = rng.randrange(1,81)
        obj = bytearray(112)
        struct.pack_into('<3H',obj,8,*position)
        struct.pack_into('<5H',obj,0x26,*rates,*angles)
        struct.pack_into('<H',obj,0x3e,speed)
        h.cpu.mem_write(h.STACK+0xda00,bytes(obj));h.setreg('BP',0xda00);h.setreg('CX',step)
        h.call(0x8daa)
        after = bytes(h.cpu.mem_read(h.STACK+0xda00,112))
        result = [*struct.unpack_from('<3H',after,8),*struct.unpack_from('<5H',after,0x26),struct.unpack_from('<H',after,0x3e)[0],*after[4:7]]
        falling.append([*position,*rates,*angles,speed,step,*result])
    from unicorn import UC_HOOK_CODE
    fired = hit = False
    def gun_hook(cpu, address, size, data):
        nonlocal fired, hit
        off = address - h.BASE
        if off == 0x84d0: hit = True
        if off == 0x6730: fired = True
        if off in (0x84d0, 0x6730, 0xcaf0):
            # Isolate gun eligibility/rays; projectile allocation is a separate consumer.
            sp = h.getreg('SP')
            h.setreg('IP', int.from_bytes(cpu.mem_read(h.STACK + sp, 2), 'little'))
            h.setreg('SP', sp + 2)
    h.cpu.hook_add(UC_HOOK_CODE, gun_hook)
    h.write(0x3142, struct.pack('<H', 0x7000))
    for i in range(1024):
        angles = [rng.randrange(-2048, 2048) & 65535 for _ in range(2)]
        target_angles = [(v + rng.randrange(-1000, 1000)) & 65535 for v in angles]
        distance = rng.randrange(7 if i < 512 else 14)
        behaviour = 0 if i < 512 else (9, 127, 255)[i % 3]
        clock = rng.choice((0,128,256,384,512,640,768,65535))
        changes = rng.choice((0,128,255))
        player_flags = rng.choice((0,0,0,16,32))
        random_state = rng.randrange(65536)
        target = [10000 + rng.randrange(-200, 201), 10000 - rng.randrange(200, 1000), 3000 + rng.randrange(-500, 500)]
        extent = rng.randrange(100, 500)
        obj = bytearray(112)
        struct.pack_into('<3H', obj, 8, 10000,10000,3000)
        struct.pack_into('<2H', obj, 0x2a, *angles)
        struct.pack_into('<H', obj, 0x48, 0xd986)
        struct.pack_into('<H', obj, 0x44, 0x1aee)
        obj[0x21] = 19
        obj[0x50] = behaviour
        h.cpu.mem_write(h.STACK+0xda00,bytes(obj))
        obj = bytearray(112);obj[7] = player_flags
        struct.pack_into('<3H',obj,8,*target);struct.pack_into('<H',obj,0x22,0x400)
        h.cpu.mem_write(h.STACK+0xd986,bytes(obj))
        h.cpu.mem_write(0x70409,struct.pack('<H',extent))
        for at,value in ((0x8c29,target_angles[1]),(0x8c37,target_angles[0]),(0xbf0,clock),(0xc50,changes),(0x92d3,random_state)):
            h.write(at,struct.pack('<H',value))
        h.setreg('DS',0x1000);h.setreg('BP',0xda00);h.setreg('DX',distance*256+31)
        h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
        fired = hit = False
        h.call(0x8afd)
        guns.append([*angles,*target_angles,distance,clock,changes,player_flags,random_state,*target,extent,
                     int(fired),int(hit),int.from_bytes(h.read(0x92d3,2),'little'),behaviour])
    lines = ['#pragma once','','// Generated by tools/generate_aircraft_combat_reference.py; native 60C8/8DAA/8AFD', '// Image SHA-256: '+IMAGE_SHA256,'','#include <array>','','namespace darker::test_reference {','']
    for name, rows in [('aircraft_sweeps',sweeps),('aircraft_falls',falling),('aircraft_guns',guns)]:
        lines += [f'inline constexpr std::array<std::array<int, {len(rows[0])}>, {len(rows)}> {name}{{{{']
        lines += ['  {'+', '.join(map(str,row))+'},' for row in rows]
        lines += ['}};','']
    lines += ['} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/aircraft_combat_samples.h').write_text('\n'.join(lines))
    print('Captured 512 extent sweeps, 512 falling-aircraft updates and 1,024 gun cases including nonzero firing settings and longer ranges.')


if __name__ == '__main__':
    main()
