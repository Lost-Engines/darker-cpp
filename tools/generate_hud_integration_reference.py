#!/usr/bin/env python3
"""Capture native engine dimming, map-coordinate encoding and Caero frame-edge blits."""
import gzip
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'tools'))
from trace_hidden_commands import Harness
from procedural_hud import ret

raw = (ROOT.parent / 'analysis/unpacked/image.bin').read_bytes()
runtime = gzip.decompress((ROOT.parent / 'analysis/hidden-commands/ui/after-command-memory.bin.gz').read_bytes())[0x1a20:0x11a20]
lines = ['#pragma once', '#include <array>', '#include <cstdint>',
         '// Generated from native 56B2, 5C65/573F and runtime-patched 5571.',
         'namespace darker::test_reference {',
         'struct engine_sample { uint16_t speed; uint8_t previous, enabled, result; };',
         'inline constexpr auto engine_samples = std::to_array<engine_sample>({']
for previous in (0,128):
    for enabled in (0,1):
        for speed in (0,199,200,201,409,410,411,65535):
            h=Harness(raw); h.setreg('BP',0xd986); h.setreg('SI',0x4552); h.setreg('AX',previous|enabled)
            h.cpu.mem_write(h.STACK+0xd9c4,struct.pack('<H',speed))
            h.cpu.emu_start(h.BASE+0x56b2,h.BASE+0x56c7,count=100)
            lines.append(f'  {{{speed},{previous},{enabled},{h.read(0x4552)[0]}}},')
lines += ['});','struct grid_sample { uint16_t column,row; uint8_t x,y; };',
          'inline constexpr auto grid_samples = std::to_array<grid_sample>({']
for cell in range(256):
    for column,row in ((cell*256+128,29080),(12672,cell*256+128)):
        h=Harness(raw); h.setreg('ES',0x6000)
        h.cpu.emu_start(h.BASE+0x396,h.BASE+0x3ab,count=5000)
        h.write(0xfdf0,struct.pack('<H',0x6000));h.cpu.mem_write(h.STACK+0xd98e,struct.pack('<HH',column,row))
        h.call(0x5c65)
        h.cpu.emu_start(h.BASE+0x573f,h.BASE+0x574f,count=100)
        x,y=h.read(0x4545,2)
        lines.append(f'  {{{column},{row},{x},{y}}},')
lines += ['});']

class Edges(Harness):
    def __init__(self):
        super().__init__(runtime);self.pixels=bytearray([99])*(320*240)
    def hook(self,cpu,address,size,data):
        off=address-self.BASE
        if off==0x55a2:
            self.setreg('IP',self.RETURN);return
        if off in (0xdc21,0xdfb0):
            x,y=self.getreg('DX'),self.getreg('DI')
            if off==0xdc21:
                y += 8 if y<168 else 0
                self.pixels[y*320+x:y*320+x+self.getreg('BP')]=bytes([self.getreg('AX')&255])*self.getreg('BP')
            else:
                sx,sy=self.getreg('CX'),self.getreg('SI')
                mask=self.read(self.getreg('AX'),2*(self.getreg('BX')&255))
                for row,(skip,width) in enumerate(zip(mask[::2],mask[1::2])):
                    source_y=sy+row+(8 if sy+row<168 else 0)
                    for dx in range(skip,skip+width):self.pixels[(y+row)*320+x+dx]=((source_y*320+sx+dx)*37+11)&255
            ret(self);return
        super().hook(cpu,address,size,data)
h=Edges();h.setreg('AX',0);h.setreg('DI',0);h.call(0x5571)
fingerprint=0xcbf29ce484222325
for value in h.pixels:fingerprint=((fingerprint^value)*0x100000001b3)&0xffffffffffffffff
lines += [f'inline constexpr uint64_t frame_edge_fingerprint{{0x{fingerprint:016x}ULL}};', '} // namespace darker::test_reference','']
(ROOT/'tests/reference/hud_integration_samples.h').write_text('\n'.join(lines))
print('Captured 32 engine states, 512 coordinate pairs and the three native cockpit-edge blits.')
