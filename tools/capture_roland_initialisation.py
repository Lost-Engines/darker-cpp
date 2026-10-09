#!/usr/bin/env python3
"""Run native 07DF and intercept the MIDI byte writer at 07A7."""
from pathlib import Path
import argparse, hashlib, json
parser = argparse.ArgumentParser(description="Capture the original LAPC-I instrument upload with Unicorn")
parser.add_argument("driver", type=Path)
parser.add_argument("output", type=Path, help="Output SysEx file")
args = parser.parse_args()
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16,UC_HOOK_CODE
from unicorn import x86_const as r
b=args.driver.read_bytes()
assert hashlib.sha256(b).hexdigest() == "1917abfeff3e0c8ea0cf6e6db6045ab07875e064fd85cc58d23c4d91823b77f8"
c=Uc(UC_ARCH_X86,UC_MODE_16);c.mem_map(0,0x100000);c.mem_write(0x10000,b)
for name,v in [('CS',0x1000),('DS',0x1000),('ES',0x1000),('SS',0x5000),('SP',0xfff0)]:c.reg_write(getattr(r,'UC_X86_REG_'+name),v)
c.mem_write(0x5fff0,b'\0\xff');out=[]
def hook(c,a,s,d):
 if a==0x107a7:
  out.append(c.reg_read(r.UC_X86_REG_AX)&255)
  sp=c.reg_read(r.UC_X86_REG_SP);ip=int.from_bytes(c.mem_read(0x50000+sp,2),'little');c.reg_write(r.UC_X86_REG_SP,sp+2);c.reg_write(r.UC_X86_REG_IP,ip)
c.hook_add(UC_HOOK_CODE,hook);c.emu_start(0x107df,0x1ff00,count=100000)
assert c.reg_read(r.UC_X86_REG_IP)==0xff00
args.output.write_bytes(bytes(out));messages=[];start=0
for end,x in enumerate(out):
 if x!=247:continue
 msg=out[start:end+1];start=end+1;assert sum(msg[5:-1])%128==0
 address=msg[5:8];data=msg[8:-2];messages.append(dict(address=address,data=data))
 print('ADDRESS',address,'length',len(data),'first bytes',data[:16])
args.output.with_suffix(".json").write_text(json.dumps(messages,indent=2))
print(len(out),'bytes',len(messages),'messages')
