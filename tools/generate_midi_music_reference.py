#!/usr/bin/env python3
"""Capture timed sampled-driver events, intercepting MIDI output or AWE32 synthesis calls."""
import hashlib
import struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_CODE
from unicorn import x86_const as r
root = Path(__file__).resolve().parents[2]
# resource, scheduler, tracks, timer, start/stop, master gain, byte outputs
profiles = [(34,0x2a3,0x8a,0x217,0x21f,0x221,0x219,(0x796,0x79f)),
            (35,0x2a4,0x8b,0x218,0x220,0x222,0x21a,(0x79e,0x7a7)),
            (36,0x296,0x81,0x20e,0x214,0x216,0x210,(0x776,0x77f)),
            (37,0xbdb,0x9c2,0xb4f,0xb57,0xb59,0xb51,())]

driver_hashes = {
    34: 'cb3682e025c4d0e4268bf6d4b23241e3a10dd5c5e6200020b6fab9b7d6d650be',
    35: '1917abfeff3e0c8ea0cf6e6db6045ab07875e064fd85cc58d23c4d91823b77f8',
    36: 'b0187b38573cdf4c225abbb227d38292eb602157f7b2e9fb95235cef8cdb502e',
    37: '00eca95020da3db30b780874e7ed26cc7bfbd0db809069a2c37a26e5458fd7d2',
}

def trace(profile, group, count, changes=None):
    n, scheduler, tracks, timer, start, stop, gain, outputs = profile
    driver = (root/f'analysis/resources/00_{n:03}.bin').read_bytes()
    assert hashlib.sha256(driver).hexdigest() == driver_hashes[n], 'Unexpected native music driver'
    source = (root/f'analysis/resources/00_{38+n-33+5*group:03}.bin').read_bytes()
    cpu = Uc(UC_ARCH_X86, UC_MODE_16); cpu.mem_map(0,0x100000)
    cpu.mem_write(0x10000,driver+source)
    if n==37: cpu.mem_write(0x10018,struct.pack("<H",len(driver)))
    for name,value in [('CS',0x1000),('DS',0x1000),('ES',0x1000),('SS',0x5000)]: cpu.reg_write(getattr(r,'UC_X86_REG_'+name),value)
    cpu.mem_write(0x10000+timer,struct.pack('<H',0x5d24))
    cpu.mem_write(0x10000+gain,b'\0\1')
    cpu.mem_write(0x10000+start,b'\1')
    for i in range(16): cpu.mem_write(0x10000+tracks+i*23+8,bytes([i]))
    events=[]; pending=[]; tick=0
    def returned(extra=0):
        sp=cpu.reg_read(r.UC_X86_REG_SP)
        dest=int.from_bytes(cpu.mem_read(0x50000+sp,2),'little')
        cpu.reg_write(r.UC_X86_REG_SP,sp+2+extra);cpu.reg_write(r.UC_X86_REG_IP,dest)
    def hook(c,a,size,data):
        ip=a-0x10000
        if ip in outputs:
            v=c.reg_read(r.UC_X86_REG_AX)&255
            if v&128: pending.clear()
            pending.append(v)
            if len(pending)==(2 if pending[0]&0xf0==0xc0 else 3):
                events.append((tick,*pending,*([] if len(pending)==3 else [0])));pending.clear()
            returned()
        elif n==37 and ip in (0x3eea,0x3e0a,0x4454,0x45fe,0x4314):
            sp=c.reg_read(r.UC_X86_REG_SP);args=struct.unpack('<HHH',bytes(c.mem_read(0x50000+sp+2,6)))
            if ip==0x4454: events.append((tick,0xc0|(args[1]&15),args[0]&255,0));returned(4)
            else:
                status={0x3eea:0x90,0x3e0a:0x90,0x45fe:0xe0,0x4314:0xb0}[ip]
                events.append((tick,status|(args[2]&15),args[1]&255,args[0]&255));returned(6)
    cpu.hook_add(UC_HOOK_CODE,hook)
    paused=False
    for tick in range(count):
        if changes and tick in changes:
            target=changes[tick]
            paused=target<0
            if paused: cpu.mem_write(0x10000+stop,b'\1')
            else:
                cpu.mem_write(0x10000+len(driver),(root/f'analysis/resources/00_{38+n-33+5*target:03}.bin').read_bytes())
                cpu.mem_write(0x10000+start,b'\1')
        elif paused: continue
        cpu.reg_write(r.UC_X86_REG_SP,0xfff0);cpu.mem_write(0x5fff0,b'\0\xff')
        cpu.emu_start(0x10000+scheduler,0x1ff00,count=200000)
        assert cpu.reg_read(r.UC_X86_REG_IP)==0xff00,(n,hex(cpu.reg_read(r.UC_X86_REG_IP)))
    return events

if __name__=='__main__':
    rows=[]
    for profile in profiles:
        for group in range(6):
            events=trace(profile,group,16384);h=0xcbf29ce484222325
            for tick,status,a,b in events:
                for value in (tick&255,tick>>8,status,a,b): h=((h^value)*0x100000001b3)&0xffffffffffffffff
            rows.append(f'  {{{profile[0]-33}, {group}, 16384, {len(events)}, 0x{h:016x}ULL}},')
            print(profile[0],group,len(events),flush=True)
    (root/'darker-cpp/tests/reference/midi_music_samples.h').write_text('#pragma once\n#include <array>\n#include <cstdint>\nnamespace darker::test_reference {\nstruct midi_music_sample { unsigned int variant, group, ticks, events; uint64_t fingerprint; };\ninline constexpr auto midi_music_samples = std::to_array<midi_music_sample>({\n'+'\n'.join(rows)+'\n});\n}\n')

    transitions=[]
    for profile in profiles:
        for pause in (False,True):
            changes={512:-1,768:1} if pause else {512:1}
            events=trace(profile,0,1280,changes);h=0xcbf29ce484222325
            for tick,status,a,b in events:
                for value in (tick&255,tick>>8,status,a,b): h=((h^value)*0x100000001b3)&0xffffffffffffffff
            transitions.append(f'  {{{profile[0]-33}, {str(pause).lower()}, {len(events)}, 0x{h:016x}ULL}},')
    (root/'darker-cpp/tests/reference/midi_transition_samples.h').write_text('#pragma once\n#include <array>\n#include <cstdint>\nnamespace darker::test_reference {\nstruct midi_transition_sample { unsigned int variant; bool pause; unsigned int events; uint64_t fingerprint; };\ninline constexpr auto midi_transition_samples = std::to_array<midi_transition_sample>({\n'+'\n'.join(transitions)+'\n});\n}\n')
