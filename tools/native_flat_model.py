"""Original flat model interpreter with only VGA plane writes replaced by span capture."""
import struct


def render(Harness, image, pool, offset, axes, base, origin, shades, dynamic, bottom=240):
    class Renderer(Harness):
        def hook(self, cpu, address, size, data):
            if address-self.BASE in (0xfc1e, 0xfc22, 0xfc32, 0xfc35, 0xfc38, 0xfc3b):
                raise ValueError(f'Non-flat command at {address-self.BASE:04x}')
            if address-self.BASE == 0xa583:
                right, left, row = self.getreg('CX'), self.getreg('SI'), self.getreg('BP')//256
                assert 0 <= left <= 320 and 0 <= right <= 320 and row < 240
                if right > left:
                    self.pixels[row*320+left:row*320+right] = self.read(0xa474)*(right-left)
                self.setreg('CX', (right-left)&65535)
                self.setreg('IP', 0xa566)
                return
            super().hook(cpu, address, size, data)
    native = Renderer(image)
    native.pixels = bytearray(320*240)
    word = lambda value: struct.pack('<H', value & 65535)
    for address, value in zip((0xfd05, 0xfd12, 0xfd1f, 0xfd31, 0xfd3e, 0xfd4b, 0xfd5d, 0xfd6a, 0xfd77), axes):
        native.write(address, word(value))
    for address, value in ((0xfcb0, base[0]), (0xfcd5, base[2]), (0xfc9b, base[4]), (0xfccd, origin[0]), (0xfcf1, origin[1])):
        native.write(address, word(value))
    for address, value in ((0xfcbf, base[1]), (0xfce4, base[3]), (0xfca8, base[5])):
        native.write(address, bytes([value]))
    native.write(0xfc13, word(0x3515))  # Original Gouraud-off dispatch to 312A.
    native.write(0x3142, word(0x1fc0))
    native.write(0xfdec, word(0x1000))
    native.write(0xfdf4, word(0xa000))
    native.write(0xa296, word(bottom))
    native.write(0x3385, word(0xe000))
    native.write(0x339a, bytes([dynamic]))
    native.write(0, struct.pack('<240H', *[y*256 for y in range(240)]))
    native.cpu.mem_write(0x20000, pool)
    native.cpu.mem_write(0x2dc00, bytes(shades))
    native.cpu.mem_write(0x8e000, bytes(shades))
    native.cpu.mem_write(0x8fffc, word(0x4400))
    for register, value in [('CS', 0x1000), ('DS', 0x1fc0), ('ES', 0x1fc0), ('SS', 0x8000),
                            ('SP', 0xfffc), ('SI', 0x400+offset+12), ('DI', 0xfc00), ('AX', 0xfc00 | pool[offset+11])]:
        native.setreg(register, value)
    native.cpu.emu_start(native.BASE+0xfc00+pool[offset+11], native.BASE+0x4400, count=500000)
    assert native.getreg('IP') == 0x4400
    return native.pixels


def fingerprint(pixels):
    result = 0xcbf29ce484222325
    for byte in pixels:
        result = ((result ^ byte)*0x100000001b3)&((1<<64)-1)
    return result
