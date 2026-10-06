#!/usr/bin/env python3
"""Capture complete Caero startup/flight callbacks with persistent state."""
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
    from trace_hidden_commands import Harness
    image = (args.workspace/'analysis/unpacked/image.bin').read_bytes()
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('Unsupported unpacked executable')
    repair = Harness(image)
    repair.cpu.mem_write(0x20000, image)
    repair.setreg('DS', 0x2000)
    class Flight(Harness):
        def hook(self, cpu, address, size, data):
            # Execute repair separately with mirrored DS, as in the existing repair
            # fixture: Unicorn loses carry when 851F modifies its active code block.
            if address-self.BASE == 0x8514:
                for at in (0x8509, 0x8518):
                    repair.write(at, self.read(at, 2))
                    repair.cpu.mem_write(0x20000+at, self.read(at, 2))
                repair.cpu.ctl_remove_cache(repair.BASE, repair.BASE+65536)
                repair.setreg('CX', self.getreg('CX'))
                repair.call(0x8514)
                for at in (0x8509, 0x8518):
                    self.write(at, bytes(repair.cpu.mem_read(0x20000+at, 2)))
                sp = self.getreg('SP')
                target = int.from_bytes(cpu.mem_read(self.STACK+sp, 2), 'little')
                self.setreg('SP', sp+2)
                self.setreg('IP', target)
                return
            super().hook(cpu, address, size, data)
    rng = random.Random(0x7eb6)
    cases = []
    for sequence in range(32):
        h = Flight(image)
        word = lambda value: struct.pack('<H', value & 65535)
        put = lambda at,value: h.cpu.mem_write(h.STACK+0xd986+at,word(value))
        patch = lambda at,value: h.write(at,word(value))
        get = lambda at: int.from_bytes(h.cpu.mem_read(h.STACK+0xd986+at,2),'little')
        value = lambda at: int.from_bytes(h.read(at,2),'little')
        h.write(0x9c4a,image[0x944a:0x964a])
        h.setreg('ES',0x6000)
        h.cpu.emu_start(h.BASE+0x396,h.BASE+0x3ab,count=5000)
        patch(0xfdf0,0x6000)
        for at in (8,10):put(at,rng.randrange(128*256))
        put(12,rng.randrange(64,5000))
        h.cpu.mem_write(h.STACK+0xd98a,bytes(rng.randrange(256) for _ in range(3)))
        for at in (0x2a,0x2c,0x2e):put(at,rng.randrange(65536))
        for at in (0x26,0x28):put(at,rng.randrange(-1024,1025))
        put(0x3e,rng.randrange(1000));put(0x40,rng.randrange(-200,201));put(0x42,rng.randrange(1000))
        put(0x66,0x7e7f if sequence<4 else 0x7eb6)
        for at,initial in ((0x80c7,rng.randrange(-256,257)),(0x7f0d,0 if sequence<4 else rng.randrange(16000)),
                           (0x7f27,rng.randrange(512)),(0x7f8f,rng.randrange(65536)),(0x7fa6,rng.randrange(0xd000)),
                           (0x8540,rng.randrange(0xc000)),(0x8518,rng.randrange(65536)),(0x8509,rng.randrange(4)*256+40),
                           (0x7e8e,rng.randrange(0x5000))):patch(at,initial)
        def state():
            return ([get(at) for at in (8,10,12)]+list(h.cpu.mem_read(h.STACK+0xd98a,3))+
                    [get(at) for at in (0x2a,0x2c,0x2e,0x3e,0x26,0x28,0x42,0x40)]+
                    [value(at) for at in (0x80c7,0x7f0d,0x7f27,0x7f8f,0x7fa6,0x8540,0x8518,0x8509,0x7e8e)]+
                    [int(get(0x66)==0x7eb6),h.read(0x4550)[0],h.read(0x4551)[0]])
        for tick in range(16):
            step=rng.choice((1,2,8,16,32,100,127))
            bank,pitch=[rng.randrange(-2048,2049)&65535 for _ in range(2)]
            engine=rng.choice((0,1,1,3));hold=int(sequence%3==0);brake=int(tick%5==0);cheat=int(sequence%7==0)
            response=416;multiplier=1656;desired=2400;reference=rng.randrange(1024);bias=-90;strength=rng.choice((0,128,255))
            h.cpu.mem_write(0x60000,bytes([1,strength])*16384)
            for at,initial in ((0x7eba,bank),(0x7ecf,pitch),(0xb9f4,0x240 if brake else 0),(0x7f20,multiplier),
                               (0x80ac,desired),(0xd5ca,reference)):
                patch(at,initial)
            h.write(0x4552,bytes([engine]));h.write(0x7ed2,bytes([hold]));h.write(0x8006,bytes([bias&255]))
            h.write(0x7f7b,b'\x89\xca' if cheat else b'\xf7\xe1')
            put(0x30,response)
            if sequence<4 and tick==8:patch(0x7f0d,0x3800)
            before=state()
            h.setreg('BP',0xd986);h.setreg('CX',step);h.setreg('DS',0x1000)
            h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
            h.call(get(0x66))
            cases.append(([step,bank,pitch,engine,hold,brake,cheat,response,multiplier,desired,reference,bias,strength],before,state()))
        print(f'Caero sequence {sequence} captured',flush=True)
    from unicorn import UC_HOOK_CODE
    boosts = []
    for reserve in (0, 8191, 8192, 49151):
        for active in (0, 255, 65535):
            h = Harness(image)
            h.write(0x8540, struct.pack('<H', reserve))
            h.write(0x7f0d, struct.pack('<H', active))
            h.setreg('SP', 0xfefe)
            h.cpu.mem_write(h.STACK+0xfefe, struct.pack('<H', h.RETURN))
            def stop(cpu, address, size, data):
                if address-h.BASE == 0xb974:
                    cpu.emu_stop()
            h.cpu.hook_add(UC_HOOK_CODE, stop)
            h.cpu.emu_start(h.BASE+0xb963, h.BASE+h.RETURN, count=1000)
            boosts.append([reserve, active, int(h.getreg('IP') == 0xb974),
                           int.from_bytes(h.read(0x8540, 2), 'little'), int.from_bytes(h.read(0x7f0d, 2), 'little')])
    lines=['#pragma once','','// Generated by tools/generate_caero_flight_reference.py; original 7E7F/7EB6 callbacks',
           '// Image SHA-256: '+IMAGE_SHA256,'// Repair helper executes native instructions with mirrored DS to avoid Unicorn self-modification carry loss',
           '','#include <array>','','namespace darker::test_reference {','',
           'struct caero_flight_sample { std::array<int, 13> input; std::array<int, 26> before, after; };',
           f'inline constexpr std::array<caero_flight_sample, {len(cases)}> caero_flight_samples{{{{']
    lines+=['  {'+', '.join('{'+', '.join(map(str,part))+'}' for part in case)+'},' for case in cases]
    lines+=['}};', '', 'struct caero_boost_sample { int reserve, active, accepted, result_reserve, result_active; };',
            'inline constexpr std::array<caero_boost_sample, 12> caero_boost_samples{{']
    lines+=['  {'+', '.join(map(str, row))+'},' for row in boosts]
    lines+=['}};','','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1]/'tests/reference/caero_flight_samples.h').write_text('\n'.join(lines))


if __name__=='__main__':
    main()
