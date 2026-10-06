#!/usr/bin/env python3
"""Capture native B409 band boundaries, replacing only the planar span writes."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys
from generate_resource_directory import IMAGE_SHA256
from native_flat_model import fingerprint


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.workspace.resolve() / 'tools'))
    from trace_hidden_commands import Harness
    image = (args.workspace / 'analysis/unpacked/image.bin').read_bytes()
    assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256

    class Background(Harness):
        def hook(self, cpu, address, size, data):
            at = address - self.BASE
            if at == 0xb519:
                start = int.from_bytes(self.read(0xb5f1, 2), 'little')
                colour = 28 if self.read(0x200)[0] < 4 else 127
                mirror = bool(self.read(0xb655)[0] & 8)
                for y in range(min(start, self.height)):
                    row = self.height - 1 - y if mirror else y
                    self.pixels[row*320:(row+1)*320] = bytes([colour])*320
            if at == 0xb534:
                # The final band fills the remaining pixels with the outer ground colour.
                cpu.emu_stop()
                return
            if at == 0xb579:
                self.colour = self.read(self.getreg('BX'))[0]
            if at == 0xb56b:
                row, count = self.getreg('DI'), self.getreg('BX')
                colour = self.getreg('AX') & 255
                for y in range(row, min(self.height, row + count)):
                    self.pixels[y*320:(y+1)*320] = bytes([colour])*320
                self.setreg('DX', 0)
                self.setreg('BP', 320)
                self.setreg('IP', 0xb574)
                return
            if at == 0xb626:
                start = self.getreg('DI')
                left = self.getreg('SI')
                if left >= 32768:
                    left -= 65536
                fraction = self.getreg('AX') >> 8
                width = int.from_bytes(self.read(0xb658, 2), 'little')
                end = int.from_bytes(self.read(0xb627, 2), 'little')
                slope = int.from_bytes(self.read(0xb652, 2), 'little')*256 + self.read(0xb64f)[0]
                mirror = bool(self.read(0xb655)[0] & 8)
                for y in range(start, end):
                    row = self.height - 1 - y if mirror else y
                    if 0 <= row < self.height:
                        lo, hi = max(0, left), min(320, left + width)
                        if lo < hi:
                            self.pixels[row*320+lo:row*320+hi] = bytes([self.colour])*(hi-lo)
                    position = left*256 + fraction - slope
                    left, fraction = divmod(position, 256)
                self.setreg('IP', 0xb69c)
                return
            super().hook(cpu, address, size, data)

    samples = []
    for height in (168, 180):
        for pitch in (0, 32, 127, 128, 384, 511, 512, 640, 896, 992):
            for roll in (0, 1, 2, 4, 16, 32, 64, 128, 255, 256, 257, 384, 480, 511, 512, 513, 640, 768, 896, 1023):
                native = Background(image)
                native.height = height
                native.colour = 0
                native.pixels = bytearray([28 if roll >= 512 else 127])*(320*height)
                native.write(0x9c4a, image[0x944a:0x964a])
                native.write(0xf003, bytes([2]))
                for at, value in ((0xfdf4, 0xa000), (0xfccd, 160), (0xfcf1, height//2), (0xa296, height), (0xa320, 320)):
                    native.write(at, struct.pack('<H', value))
                native.setreg('BX', pitch*2)
                native.setreg('DI', roll*2)
                native.setreg('SP', 0xfefe)
                native.cpu.mem_write(native.STACK+0xfefe, struct.pack('<H', native.RETURN))
                native.cpu.emu_start(native.BASE+0xb409, native.BASE+native.RETURN, count=20000)
                assert native.getreg('IP') in (0xb534, native.RETURN), hex(native.getreg('IP'))
                samples.append((height, pitch*64, roll*64, fingerprint(native.pixels)))
    lines = ['#pragma once', '', '// Generated from native B409 band setup; planar writes replaced by clipped span capture',
             '// Final outer band supplied as the initial fill; this does not assert VGA write ordering',
             '', '#include <array>', '#include <cstdint>', '', 'namespace darker::test_reference {', '',
             'struct sky_ground_sample { int height; std::uint16_t pitch, roll; std::uint64_t hash; };',
             f'inline constexpr std::array<sky_ground_sample, {len(samples)}> sky_ground_samples{{{{']
    lines += [f'  {{{h}, {p}, {r}, 0x{v:016x}}},' for h,p,r,v in samples]
    lines += ['}};', '', '} // namespace darker::test_reference', '']
    (Path(__file__).resolve().parents[1] / 'tests/reference/sky_ground_samples.h').write_text('\n'.join(lines))
    print(f'Captured {len(samples)} native backgrounds')


if __name__ == '__main__':
    main()
