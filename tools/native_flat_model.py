"""Original flat model interpreter with only VGA plane writes replaced by span capture."""
import struct
from unicorn import UC_HOOK_INSN, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_INS_OUT


def make_renderer(Harness, image):
    class Renderer(Harness):
        def hook(self, cpu, address, size, data):
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
    mask = [0]
    direct_colour = [False]
    def output(cpu, port, size, value, data):
        if port == 0x3cf:
            direct_colour[0] = value == 255
        if port == 0x3c5:
            mask[0] = value & 15
    def write(cpu, access, address, size, value, data):
        if 0xa0000 <= address < 0xb0000:
            for byte in range(size):
                for plane in range(4):
                    if mask[0] >> plane & 1:
                        offset = address-0xa0000+byte
                        x, y = (offset%256)*4+plane, offset//256
                        if 0 <= x < 320 and 0 <= y < 240:
                            native.pixels[y*320+x] = ((value >> (byte*8)) & 255) if direct_colour[0] else native.read(0xa474)[0]
    native.cpu.hook_add(UC_HOOK_INSN, output, None, 1, 0, UC_X86_INS_OUT)
    native.cpu.hook_add(UC_HOOK_MEM_WRITE, write)
    return native


def render(Harness, image, pool, offset, axes, base, origin, shades, dynamic, bottom=240, near=False, parameters=None, state=0, flat=True):
    native = make_renderer(Harness, image)
    word = lambda value: struct.pack('<H', value & 65535)
    for address, value in zip((0xfd05, 0xfd12, 0xfd1f, 0xfd31, 0xfd3e, 0xfd4b, 0xfd5d, 0xfd6a, 0xfd77), axes):
        native.write(address, word(value))
    for address, value in ((0xfcb0, base[0]), (0xfcd5, base[2]), (0xfc9b, base[4]), (0xfccd, origin[0]), (0xfcf1, origin[1])):
        native.write(address, word(value))
    for address, value in ((0xfcbf, base[1]), (0xfce4, base[3]), (0xfca8, base[5])):
        native.write(address, bytes([value]))
    if flat: native.write(0xfc13, word(0x3515))  # Original Gouraud-off dispatch to 312A.
    native.write(0x3142, word(0x1fc0))
    native.write(0xfdec, word(0x1000))
    native.write(0xfdf4, word(0xa000))
    native.write(0xa296, word(bottom))
    segment = 0x1fe0 if near else 0x1fc0
    page = 0xfe00 if near else 0xfc00
    if near:
        for address, value in ((0xfeb0, base[0]), (0xfed5, base[2]), (0xfe9b, base[4]),
                               (0x2148, origin[0]), (0x2158, origin[1]), (0x2226, origin[0]), (0x2236, origin[1]), (0x2385, origin[0]), (0x236f, origin[1])):
            native.write(address, word(value))
        for address, value in ((0xfebf, base[1]), (0xfee4, base[3]), (0xfea8, base[5])):
            native.write(address, bytes([value]))
        if flat: native.write(0xfe13, word(0x32a8))  # Original near-path Gouraud-off dispatch.
        native.write(0x30df, word(segment))
        native.write(0xffec, word(0x1000))
    native.write(0x3385, word(0xe000))
    native.write(0xa9a1, word(0xe000))
    native.write(0x339a, bytes([dynamic]))
    native.write(0x319f, word(state << 8))
    native.cpu.mem_write(0x87935, struct.pack('<256H', *[(value&65535) for value in (parameters or [0]*256)]))
    for address, value in ((0x3057, origin[0]), (0x307f, origin[0]), (0x3040, origin[1]), (0x317b, origin[0]), (0x318a, origin[1])):
        native.write(address, word(value))
    native.write(0, struct.pack('<240H', *[y*256 for y in range(240)]))
    native.cpu.mem_write(0x20000, pool)
    native.cpu.mem_write(segment*16+0xe000, bytes(shades))
    native.cpu.mem_write(0x8e000, bytes(shades))
    native.cpu.mem_write(0x8fffc, word(0x4400))
    for register, value in [('CS', 0x1000), ('DS', segment), ('ES', segment), ('SS', 0x8000),
                            ('SP', 0xfffc), ('SI', 0x20000-segment*16+offset+12), ('DI', 0xfc00), ('AX', page | pool[offset+11])]:
        native.setreg(register, value)
    native.cpu.emu_start(native.BASE+page+pool[offset+11], native.BASE+0x4400, count=500000)
    assert native.getreg('IP') == 0x4400
    return native.pixels


def fingerprint(pixels):
    result = 0xcbf29ce484222325
    for byte in pixels:
        result = ((result ^ byte)*0x100000001b3)&((1<<64)-1)
    return result
