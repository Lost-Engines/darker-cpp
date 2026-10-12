#!/usr/bin/env python3
"""Capture native AFA2 fade gains and AFAD component lookup tables."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys
from generate_resource_directory import IMAGE_SHA256
p=argparse.ArgumentParser(description=__doc__);p.add_argument('workspace',type=Path);a=p.parse_args()
sys.path.insert(0,str(a.workspace/'tools'))
from trace_hidden_commands import Harness
b=(a.workspace/'analysis/unpacked/image.bin').read_bytes();assert hashlib.sha256(b).hexdigest()==IMAGE_SHA256
class Probe(Harness):
    capture=False
    def hook(self,cpu,address,size,data):
        if self.capture and address-self.BASE==0xafad:
            self.gain=self.getreg('BX');self.setreg('IP',self.RETURN);return
        super().hook(cpu,address,size,data)
h=Probe(b);gains=[]
for phase in range(512):
    h.capture=True;h.setreg('BX',phase);h.call(0xafa2);gains.append(h.gain)
h.capture=False;fingerprints=[]
for gain in range(65):
    h.setreg('BX',gain);h.call(0xafad)
    rgb=bytes(h.cpu.mem_read(h.STACK,768));assert rgb[:256]==rgb[256:512]==rgb[512:]
    fingerprint=0xcbf29ce484222325
    for value in rgb[:256]:fingerprint=((fingerprint^value)*0x100000001b3)&0xffffffffffffffff
    fingerprints.append(fingerprint)
lines=['#pragma once','','// Native AFA2/AFAD; image SHA-256: '+IMAGE_SHA256,'#include <array>','#include <cstdint>','namespace darker::test_reference {','inline std::array<uint8_t,512> constexpr fade_gains{']
lines+=['  '+', '.join(map(str,gains[i:i+32]))+',' for i in range(0,512,32)]
lines+=['};','inline std::array<uint64_t,65> constexpr fade_components{']
lines+=['  '+', '.join(hex(v)+'ULL' for v in fingerprints[i:i+4])+',' for i in range(0,65,4)]
lines+=['};','} // namespace darker::test_reference','']
(Path(__file__).resolve().parents[1]/'tests/reference/palette_fade_samples.h').write_text('\n'.join(lines))
