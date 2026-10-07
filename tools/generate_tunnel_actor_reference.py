#!/usr/bin/env python3
"""Run complete native underground motion with original maps, route geometry and nearby aircraft."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from generate_resource_directory import IMAGE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace',type=Path)
    parser.add_argument('--scripts',action='store_true')
    args = parser.parse_args()
    root = args.workspace.resolve()
    sys.path.insert(0,str(root / 'tools'))
    from verify_player_collisions import setup
    from unicorn import UC_HOOK_CODE
    raw = (root / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == IMAGE_SHA256
    h,bank,_ = setup(32)
    h.write(0xfdf2,struct.pack('<H',0x4000))
    h.cpu.mem_write(0x80000,(root / 'analysis/resources/00_078.bin').read_bytes())
    h.setreg('DS',0x8000)
    h.setreg('SI',0)
    h.setreg('SP',0xfefe)
    h.cpu.mem_write(h.STACK+0xfefe,struct.pack('<H',h.RETURN))
    h.setreg('EFLAGS',2)
    h.cpu.emu_start(h.BASE+0xd0be,h.BASE+h.RETURN,count=1000000)
    assert h.getreg('IP') == h.RETURN
    cells = bytearray(32768)
    cells[::2] = (root / 'analysis/resources/00_070.bin').read_bytes()
    h.cpu.mem_write(0x60000,bytes(cells))
    record = next(r for r in json.loads((root / 'analysis/missions/scenarios.json').read_text())['records']
                  if r['resource'] == '04_002' and r['record'] == 0)
    objects = record['objects'][:3]
    if args.scripts:h.write(0xf000,bytes.fromhex(record['shared_hex']))
    bases = [0xd986 + i*112 for i in (1,2,3)]
    starts = []
    for i,(bp,obj) in enumerate(zip(bases,objects)):
        definition = 0x1926 + obj['definition_slot']*24
        model = bank['special_offsets'][obj['definition_slot']] + 0x400
        height = struct.unpack('<h',h.cpu.mem_read(0x70000+model+7,2))[0]
        h.write(definition,struct.pack('<H',model))
        state = bytearray(112)
        struct.pack_into('<HH',state,0,bases[i-1] if i else 0,bases[i+1] if i<2 else 0)
        state[7] = 2
        state[0x20:0x22] = bytes((i+1,obj['definition_slot']))
        struct.pack_into('<H',state,0x3e,raw[definition+9]*16)
        state[0x50:0x56] = bytes.fromhex(obj['runtime_50_55_hex'])
        h.cpu.mem_write(h.STACK+bp,bytes(state))
        h.setreg('BP',bp)
        h.setreg('BX',definition)
        h.call(0xbf41)
        h.setreg('DX',obj['position_words'][0])
        h.setreg('BX',obj['position_words'][1])
        h.setreg('DI',obj['heading_word'])
        h.setreg('SI',0xf000)
        h.call(0xbee7)
        x,y,z = h.getreg('DX'),h.getreg('BX'),(h.getreg('CX')-height)&65535
        h.cpu.mem_write(h.STACK+bp+8,struct.pack('<3H',x,y,z))
        cell = (x>>8)|(y&0xff00)
        h.cpu.mem_write(h.STACK+bp+0x4a,struct.pack('<H',cell))
        h.cpu.mem_write(h.STACK+bp+0x62,struct.pack('<H',cell))
        starts.append(cell)
        if args.scripts:
            cursor = 0xf000 + obj['script_offset'] - record['shared_offset']
            h.cpu.mem_write(h.STACK+bp+0x58,struct.pack('<HH',cursor,cursor))
            h.cpu.mem_write(h.STACK+bp+7,b'\x02')
    h.write(0x6fd8,struct.pack('<HH',bases[-1],bases[0]))
    h.write(0x6fc8,struct.pack('<H',0xd986))
    h.cpu.mem_write(h.STACK+0xd986,bytes(112))
    h.cpu.mem_write(h.STACK+0xd986+8,struct.pack('<3H',13952,14464,512))
    def hook(cpu,address,size,data):
        if address == h.BASE + 0xbf97:
            sp = h.getreg('SP')
            h.setreg('IP',int.from_bytes(cpu.mem_read(h.STACK+sp,2),'little'))
            h.setreg('SP',sp+2)
    if not args.scripts:h.cpu.hook_add(UC_HOOK_CODE,hook)
    rows = []
    for frame in range(512):
        h.write(0xbf8,struct.pack("<H",frame*8))
        h.write(0xbf0,struct.pack("<H",frame*8))
        if frame == 128 and not args.scripts:
            for i,bp in enumerate(bases):
                h.cpu.mem_write(h.STACK+bp+0x4a,struct.pack('<H',starts[(i+1)%3]))
        if frame >= 256 and not args.scripts:
            position = list(struct.unpack('<3H',h.cpu.mem_read(h.STACK+bases[0]+8,6)))
            position[0] = (position[0]+100)&65535
            h.cpu.mem_write(h.STACK+0xd986+8,struct.pack('<3H',*position))
        for bp in reversed(bases):
            h.setreg('BP',bp)
            h.setreg('CX',8)
            h.setreg('DS',0x1000)
            h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536)
            h.call(0x8609)
            state = bytes(h.cpu.mem_read(h.STACK+bp,112))
            values = [int.from_bytes(state[o:o+2],'little') for o in (8,10,12,0x2a,0x2c,0x2e,0x3e,0x26,0x28,0x56,0x64,0x62,0x4a,0x60)]
            row = [*values,state[0x6e],state[0x47],*state[4:7]]
            if args.scripts:
                cursor,checkpoint,_,deadline = struct.unpack_from('<4H',state,0x58)
                row += [state[7],deadline,65535 if cursor == 0xc50f else (cursor-0xf000)&65535,(checkpoint-0xf000)&65535]
            rows.append(row)
    label = 'tunnel_scripted_actor' if args.scripts else 'tunnel_actor'
    lines = ['#pragma once','','// Generated by generate_tunnel_actor_reference.py; native 8609 with real routes and air-list traversal.',
             '// Original BF97 scripting executes against the scenario.' if args.scripts else '// Only BF97 scripting is substituted; target changes are controlled explicitly.',
             '#include <array>','#include <cstdint>','','namespace darker::test_reference {','',
             f'inline constexpr std::array<std::array<uint16_t,{23 if args.scripts else 19}>,{len(rows)}> {label}_samples{{{{']
    lines += ['  {'+','.join(map(str,row))+'},' for row in rows]
    lines += ['}};','','} // namespace darker::test_reference','']
    (Path(__file__).resolve().parents[1] / f'tests/reference/{label}_samples.h').write_text('\n'.join(lines))
    print(f'Captured {len(rows)} complete native underground actor updates.')


if __name__ == '__main__':
    main()
