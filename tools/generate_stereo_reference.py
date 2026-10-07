#!/usr/bin/env python3
"""Capture native camera-space stereo carrier attenuation before hardware writes."""
import argparse
import hashlib
from pathlib import Path
import random
import struct
import sys
from generate_resource_directory import IMAGE_SHA256
p=argparse.ArgumentParser(description=__doc__);p.add_argument('workspace',type=Path);a=p.parse_args()
sys.path.insert(0,str(a.workspace/'tools'))
from trace_hidden_commands import Harness
image=(a.workspace/'analysis/unpacked/image.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()==IMAGE_SHA256
class Probe(Harness):
    def hook(self,cpu,address,size,data):
        if address-self.BASE==0x11cc:
            self.result=self.getreg('BX');self.setreg('IP',self.RETURN);return
        super().hook(cpu,address,size,data)
h=Probe(image);rng=random.Random(0x3a2a);rows=[]
for i in range(512):
    delta=[rng.randrange(-4096,4096) for _ in range(3)]
    coefficients=[rng.randrange(-32768,32768) for _ in range(6)]
    for address,value in zip([0x2e61,0x2e58,0x2e4f,0x2e8c,0x2e95,0x2e9e],coefficients):h.write(address,struct.pack('<h',value))
    level=rng.randrange(65536)
    for reg,value in zip(['AX','BX','CX','DX','DI'],[*delta,min(65535,level+(level>>4)),0x10ac]):h.setreg(reg,value&65535)
    h.cpu.ctl_remove_cache(h.BASE,h.BASE+65536);h.call(0x3a2a)
    rows.append([*delta,*coefficients,level,h.result&255,h.result>>8])
lines=['#pragma once','','// Native 3A2A; image SHA-256: '+IMAGE_SHA256,'#include <array>','namespace darker::test_reference {','inline std::array<std::array<int,12>,512> constexpr stereo_samples{{']
lines+=['  {'+', '.join(map(str,r))+'},' for r in rows];lines+=['}};','} // namespace darker::test_reference','']
(Path(__file__).resolve().parents[1]/'tests/reference/stereo_samples.h').write_text('\n'.join(lines))
