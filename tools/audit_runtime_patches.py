#!/usr/bin/env python3
"""Inventory direct writes in bounded setup/frame ranges and execute world-profile patches.

Requires Capstone and Unicorn. This is not a whole-program disassembly: indirect
writes, loaded native scenario blocks and consumers outside these ranges remain
separate audit work.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
from generate_resource_directory import IMAGE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    image = (workspace / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16, CS_AC_WRITE
    from capstone.x86 import X86_OP_MEM, X86_REG_SS
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    writes = []
    ranges = [('mission and frame setup', 0x3c14, 0x3e96),
              ('world renderer setup', 0xbc44, 0xbc99),
              ('world profile', 0xbca4, 0xbccb),
              ('player flight parameters', 0xbd16, 0xbd25),
              ('mission exit', 0x3ec8, 0x3f41),
              ('automatic return completion', 0x7d63, 0x7d70)]
    for name, start, end in ranges:
        for instruction in decoder.disasm(image[start:end], start):
            for operand in instruction.operands:
                if operand.type != X86_OP_MEM or not operand.access & CS_AC_WRITE:
                    continue
                memory = operand.mem
                if memory.base or memory.index or memory.segment == X86_REG_SS:
                    continue
                writes.append(dict(region=name, writer=f'{instruction.address:04X}',
                                   instruction=f'{instruction.mnemonic} {instruction.op_str}',
                                   destination=f'{memory.disp & 65535:04X}', size=operand.size))
    sys.path.insert(0, str(workspace / 'tools'))
    from trace_hidden_commands import Harness
    from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
    profiles = []
    for mode, name in enumerate(('Delphi', 'Halon', 'Underground')):
        native = Harness(image)
        native.write(0x6202, bytes([mode]))
        observed = []
        def record(cpu, access, address, size, value, data):
            if native.BASE <= address < native.BASE + 65536:
                observed.append(dict(destination=f'{address-native.BASE:04X}', size=size,
                                     value=f'{value:0{size*2}X}'))
        def stop(cpu, address, size, data):
            if address == native.BASE + 0xbccb:
                native.setreg('IP', native.RETURN)
        native.cpu.hook_add(UC_HOOK_MEM_WRITE, record)
        native.cpu.hook_add(UC_HOOK_CODE, stop)
        native.call(0xbca4)
        assert len(observed) == 7
        profiles.append(dict(mode=mode, name=name, writes=observed))
    returns = []
    for end_marker in (0xfe, 0xff):
        for outstanding in (0, 1, 255):
            native = Harness(image)
            # C84F is the patched address of the current building-objective cursor.
            native.write(0xc84f, (0xe000).to_bytes(2, 'little'))
            native.write(0xe000, bytes([end_marker]))
            native.write(0xc16f, bytes([outstanding]))
            native.write(0x3e9a, b'\0')
            native.call(0x7d63)
            outcome = native.read(0x3e9a)[0]
            assert outcome == (1 if end_marker == 0xff and outstanding == 0 else 3)
            returns.append(dict(building_cursor=f'{end_marker:02X}',
                                outstanding_objects=outstanding, outcome=outcome))
    report = dict(image_sha256=IMAGE_SHA256,
                  scope='Direct absolute writes in the listed bounded ranges; not a full self-modification census.',
                  ranges=[dict(name=n, start=f'{s:04X}', end_exclusive=f'{e:04X}') for n,s,e in ranges],
                  direct_writes=writes, executed_world_profiles=profiles, executed_return_decisions=returns)
    destination = Path(__file__).resolve().parents[1] / 'docs/runtime-patch-audit.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Inventoried {len(writes)} direct writes; executed all seven profile writes in each of three worlds and six return decisions.')


if __name__ == '__main__':
    main()
