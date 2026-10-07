#!/usr/bin/env python3
"""Capture native D510/D5C9 player tunnel motion, including route following and free flight."""
import argparse
import hashlib
from pathlib import Path
import random
import struct
import sys
from generate_resource_directory import IMAGE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace',type=Path)
    root = parser.parse_args().workspace.resolve()
    sys.path.insert(0,str(root / 'tools'))
    from trace_hidden_commands import Harness
    from verify_player_collisions import setup
    raw = (root / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == IMAGE_SHA256
    repair = Harness(raw)
    repair.cpu.mem_write(0x20000,raw)
    repair.setreg('DS',0x2000)
    class Flight(Harness):
        def hook(self,cpu,address,size,data):
            if address-self.BASE == 0x8514:
                for at in (0x8509,0x8518):
                    repair.write(at,self.read(at,2))
                    repair.cpu.mem_write(0x20000+at,self.read(at,2))
                repair.cpu.ctl_remove_cache(repair.BASE,repair.BASE+65536)
                repair.setreg('CX',self.getreg('CX'));repair.call(0x8514)
                for at in (0x8509,0x8518):self.write(at,bytes(repair.cpu.mem_read(0x20000+at,2)))
                sp = self.getreg('SP')
                self.setreg('IP',int.from_bytes(cpu.mem_read(self.STACK+sp,2),'little'))
                self.setreg('SP',sp+2)
                return
            super().hook(cpu,address,size,data)
    h = Flight(raw)
    h.write(0x9c4a,raw[0x944a:0x984a])
    h.write(0xfdf2,struct.pack('<H',0x4000))
    h.write(0xfdf0,struct.pack('<H',0x3000))
    h.cpu.mem_write(0x70000,(root / 'analysis/resources/00_078.bin').read_bytes())
    h.setreg('DS',0x7000);h.setreg('SI',0);h.setreg('SP',0xfefe)
    h.cpu.mem_write(h.STACK+0xfefe,struct.pack('<H',h.RETURN))
    h.cpu.emu_start(h.BASE+0xd0be,h.BASE+h.RETURN,count=1000000)
    assert h.getreg('IP') == h.RETURN
    cells = bytearray(32768)
    cells[::2] = (root / 'analysis/resources/00_070.bin').read_bytes()
    h.cpu.mem_write(0x30000,bytes(cells))
    directory,bank,_ = setup(32)
    h.cpu.mem_write(0x3a000,bytes(directory.cpu.mem_read(0x6a000,2048)))
    word = lambda value: struct.pack('<H',value & 65535)
    put = lambda at,value: h.cpu.mem_write(h.STACK+0xd986+at,word(value))
    patch = lambda at,value: h.write(at,word(value))
    get = lambda at: int.from_bytes(h.cpu.mem_read(h.STACK+0xd986+at,2),'little')
    value = lambda at: int.from_bytes(h.read(at,2),'little')
    def state():
        return ([get(at) for at in (8,10,12,0x2a,0x2c,0x2e,0x3e,0x24,0x26,0x28,0x42,0x40,0x62,0x60)]
                + [h.cpu.mem_read(h.STACK+0xd986+0x6e,1)[0]] + list(h.cpu.mem_read(h.STACK+0xd98a,3))
                + [value(at) for at in (0xd5cd,0xd632,0xd72a,0xd805,0x8540)] + [h.read(0x4550)[0]]
                + [int(h.read(0xd52f)[0] != 0)]
                + [int.from_bytes(h.cpu.mem_read(h.STACK+0xf40+at,2),'little') for at in (0x2a,0x2c,0x24,0x26)]
                + [value(0x7fa6),h.read(0x4551)[0],value(0x8518),value(0x8509)])
    rng = random.Random(0xd5c9)
    cases = []
    for sequence in range(64):
        h.cpu.mem_write(h.STACK+0xd986,bytes(112))
        h.setreg('BP',0xd986);h.setreg('DX',13952);h.setreg('BX',14464);h.setreg('DI',0);h.setreg('SI',0xf000)
        h.call(0xbee7)
        x,y,z = h.getreg('DX'),h.getreg('BX'),h.getreg('CX')
        for at,v in ((8,x),(10,y),(12,z),(0x2a,h.getreg('DI')),(0x62,(x>>8)|(y&0xff00)),(0x30,176)):
            put(at,v)
        for at in (0xd5cd,0xd632,0xd72a,0xd805):patch(at,0)
        patch(0x8540,8191)
        for at in (0x7fa6,0x8518):patch(at,0)
        patch(0x8509,280)
        h.write(0xd52f,b'\0')
        h.write(0x4551,b'\0')
        h.cpu.mem_write(h.STACK+0xf40,bytes(112))
        h.cpu.mem_write(h.STACK+0xf70,word(480))
        for frame in range(32):
            step = (1,2,8,16)[sequence%4]
            pitch,bank = (0,0) if sequence<4 else (rng.randrange(-2048,2049),rng.randrange(-2048,2049))
            engine = not (sequence%3 == 1 and frame > 12)
            brake = frame%9 == 8
            forward = (248,496,744,992)[sequence%4]
            pitch_drive = (pitch*step >> 8) & 65535
            patch(0x7ecf,pitch_drive)
            patch(0xd5ca,pitch);patch(0xd62d,bank);patch(0x7f20,forward)
            patch(0xb9f4,0x240 if brake else 0)
            h.write(0x4552,bytes([int(engine)]))
            cell = get(0x62)
            tile = cells[(cell >> 8)*256 + ((cell & 127)*2)]
            marker = h.cpu.mem_read(0x3a000 + tile*8 + 4,1)[0]
            before = state()
            h.setreg('BP',0xd986);h.setreg('CX',step);h.setreg('DS',0x1000)
            h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
            h.call(0xd5c9 if sequence < 32 else 0xd510)
            assert h.getreg('IP') == h.RETURN
            cases.append(([step,pitch&65535,bank&65535,forward,176,int(engine),int(brake),pitch_drive,int(sequence >= 32),marker],before,state()))
    lines = ['#pragma once','','// Generated by generate_tunnel_flight_reference.py; native D510/D5C9 with original map, routes and geometry directory.',
             '#include <array>','#include <cstdint>','','namespace darker::test_reference {','',
             'struct tunnel_flight_sample { std::array<uint16_t,10> input; std::array<uint16_t,33> before, after; };',
             f'inline constexpr std::array<tunnel_flight_sample,{len(cases)}> tunnel_flight_samples{{{{']
    lines += ['  {'+','.join('{'+','.join(map(str,part))+'}' for part in case)+'},' for case in cases]
    lines += ['}};','','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1] / 'tests/reference/tunnel_flight_samples.h').write_text('\n'.join(lines))
    print(f'Captured {len(cases)} native player tunnel movement samples.')


if __name__ == '__main__':
    main()
