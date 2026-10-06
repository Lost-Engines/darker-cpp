#!/usr/bin/env python3
"""Run the original SB sequencer; intercept OPL writes and compare complete timed streams."""
import sys,struct,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16,UC_HOOK_CODE
from unicorn import x86_const as r
root=Path(__file__).resolve().parents[2];driver=(root/'analysis/resources/00_033.bin').read_bytes()
def trace(group,count,changes=None):
 source=(root/f'analysis/resources/00_{38+5*group:03}.bin').read_bytes()
 cpu=Uc(UC_ARCH_X86,UC_MODE_16);cpu.mem_map(0,0x100000);cpu.mem_write(0x10000,driver+source)
 for name,value in [('CS',0x1000),('DS',0x1000),('ES',0x1000),('SS',0x5000)]:cpu.reg_write(getattr(r,'UC_X86_REG_'+name),value)
 cpu.mem_write(0x10209,struct.pack('<H',0x5d24));cpu.mem_write(0x10211,b'\x01')
 for i in range(16):cpu.mem_write(0x1007c+i*23+8,bytes([i]))
 writes=[];tick=0
 p=0x1af2
 while (w:=int.from_bytes(driver[p:p+2],'little')):
  cpu.mem_write(0x1138e+(w&255),bytes([w>>8]));writes.append([0,w&255,w>>8]);p+=2
 def hook(c,a,size,data):
  ip=a-0x10000
  if ip in (0x856,0x87e):
   ax=c.reg_read(r.UC_X86_REG_AX);reg=ax&255;value=ax>>8
   if ip==0x87e or c.mem_read(0x1138e+reg,1)[0]!=value:
    writes.append([tick,reg,value]);c.reg_write(r.UC_X86_REG_AX,value*257)
   c.mem_write(0x1138e+reg,bytes([value]))
   sp=c.reg_read(r.UC_X86_REG_SP);dest=int.from_bytes(c.mem_read(0x50000+sp,2),'little');c.reg_write(r.UC_X86_REG_SP,sp+2);c.reg_write(r.UC_X86_REG_IP,dest)
 cpu.hook_add(UC_HOOK_CODE,hook)
 paused=False
 for tick in range(count):
  if changes and tick in changes:
   selection=changes[tick]
   cpu.mem_write(0x10213,b'\x01')
   paused=selection<0
   if not paused:
    cpu.mem_write(0x12c40,(root/f'analysis/resources/00_{38+5*selection:03}.bin').read_bytes())
    cpu.mem_write(0x10211,b'\x01');cpu.mem_write(0x1138e,bytes([255])*256)
    p=0x1af2
    while (w:=int.from_bytes(driver[p:p+2],'little')):
     cpu.mem_write(0x1138e+(w&255),bytes([w>>8]));writes.append([tick,w&255,w>>8]);p+=2
  elif paused:continue
  cpu.reg_write(r.UC_X86_REG_SP,0xfff0);cpu.mem_write(0x5fff0,b'\0\xff');cpu.emu_start(0x10295,0x1ff00,count=100000)
  assert cpu.reg_read(r.UC_X86_REG_IP)==0xff00
 return writes
def fingerprint(writes):
 result=0xcbf29ce484222325
 for tick,register,value in writes:
  for byte in (tick&255,tick>>8,register,value):result=((result^byte)*0x100000001b3)&0xffffffffffffffff
 return result

if __name__=='__main__':
 rows=[];transitions=[]
 for group in range(6):
  writes=trace(group,16384)
  rows.append(f'  {{{38+5*group}, 16384, {len(writes)}, 0x{fingerprint(writes):016x}ULL}},')
  print(group,len(writes),flush=True)
  for pause in (False,True):
   target=(group+1)%6
   changes={512:-1,768:target} if pause else {512:target}
   writes=trace(group,1280,changes)
   transitions.append(f'  {{{group}, {target}, {str(pause).lower()}, {len(writes)}, 0x{fingerprint(writes):016x}ULL}},')
 output='#pragma once\n#include <array>\n#include <cstdint>\n\nnamespace darker::test_reference {\nstruct music_sample { unsigned int resource, ticks, writes; uint64_t fingerprint; };\ninline constexpr auto music_samples = std::to_array<music_sample>({\n'+'\n'.join(rows)+'\n});\n'
 output+='struct music_transition_sample { unsigned int source, target; bool pause; unsigned int writes; uint64_t fingerprint; };\ninline constexpr auto music_transition_samples = std::to_array<music_transition_sample>({\n'+'\n'.join(transitions)+'\n});\n} // namespace darker::test_reference\n'
 (root/'darker-cpp/tests/reference/music_samples.h').write_text(output)
