"""Memory card icon for the port (icon.sys + jkps2.ico) with an ORIGINAL character
(a little orange jumper drawn here, not taken from the game) that jumps and falls.
Usage: python -I tools/make_icon.py src/icon   -> icon.sys, jkps2.ico
PS2 icon format: header, vertices per animation shape, normals, UV, colors, animation and
128x128 RGB5A1 texture."""
import os, struct, sys
from PIL import Image, ImageDraw

W = 128


def draw_texture():
    """Original character: orange body, big eyes, brown feet and a leaf on its head."""
    img = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((24, 34, 104, 112), fill=(240, 130, 40, 255), outline=(120, 50, 10, 255), width=4)  # body
    d.ellipse((34, 44, 70, 70), fill=(255, 190, 110, 255))                                        # shine
    for ex in (46, 74):                                                                          # eyes
        d.ellipse((ex, 56, ex + 16, 76), fill=(255, 255, 255, 255), outline=(40, 20, 10, 255), width=2)
        d.ellipse((ex + 6, 62, ex + 13, 72), fill=(20, 20, 30, 255))
    d.arc((52, 74, 84, 94), 20, 160, fill=(90, 30, 10, 255), width=4)                           # smile
    d.ellipse((30, 104, 58, 122), fill=(110, 70, 30, 255))                                       # feet
    d.ellipse((70, 104, 98, 122), fill=(110, 70, 30, 255))
    d.polygon([(64, 36), (54, 14), (76, 8), (70, 30)], fill=(70, 170, 60, 255))                  # leaf
    d.line((64, 36, 66, 22), fill=(40, 110, 40, 255), width=3)
    return img


def rgb5a1(img):
    out = bytearray()
    for y in range(W):
        for x in range(W):
            r, g, b, a = img.getpixel((x, y))
            out += struct.pack('<H', (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10 | (0x8000 if a >= 128 else 0))
    return bytes(out)


def write_ico(path, tex):
    # textured quad (2 triangles); 3 shapes: crouched, jumping up and falling
    shapes = [
        (-1.6, 1.6, -2.4, 0.0),   # crouched: low and wide
        (-1.2, 1.2, -4.6, -1.4),  # jumping: up and stretched
        (-1.4, 1.4, -3.6, -0.6),  # falling: halfway down
    ]
    quad_uv = [(0, 0), (1, 0), (0, 1), (1, 0), (1, 1), (0, 1)]
    out = bytearray(struct.pack('<IIIfI', 0x010000, len(shapes), 0x07, 1.0, 6))
    for u, v in quad_uv:
        for x0, x1, ytop, ybot in shapes:
            x = x0 if u == 0 else x1
            y = ytop if v == 0 else ybot
            out += struct.pack('<hhhh', int(x * 4096), int(y * 4096), 0, 0)
        out += struct.pack('<hhhh', 0, 0, -4096, 0)                   # normal toward the camera
        out += struct.pack('<hh', int(u * 4096), int(v * 4096))       # UV
        out += struct.pack('<BBBB', 128, 128, 128, 128)               # color
    # animation: each shape's weight rises and falls in turn (crouch -> jump -> fall -> crouch)
    length = 60
    out += struct.pack('<IIfII', 0x01, length, 1.0, 0, len(shapes))
    keys = {0: [(0, 1), (20, 0), (40, 0), (60, 1)],
            1: [(0, 0), (20, 1), (40, 0), (60, 0)],
            2: [(0, 0), (20, 0), (40, 1), (60, 0)]}
    for sid in range(len(shapes)):
        out += struct.pack('<IIII', sid, len(keys[sid]), 0, 0)
        for t, val in keys[sid]:
            out += struct.pack('<ff', float(t), float(val))
    out += rgb5a1(tex)
    open(path, 'wb').write(bytes(out))


def sjis_full(text):
    """Browser title: full-width characters in Shift-JIS."""
    full = ''.join('　' if c == ' ' else chr(ord(c) + 0xFEE0) if 0x21 <= ord(c) <= 0x7E else c for c in text)
    return full.encode('shift_jis')


def write_icon_sys(path, line1, line2):
    t1, t2 = sjis_full(line1), sjis_full(line2)
    title = (t1 + t2)[:66]
    b = bytearray(964)
    struct.pack_into('<4sHHI', b, 0, b'PS2D', 0, len(t1), 0)
    struct.pack_into('<I', b, 12, 0x40)                                       # background opacity
    for i, (r, g, bl) in enumerate(((40, 30, 80), (40, 30, 80), (10, 10, 30), (10, 10, 30))):
        struct.pack_into('<IIII', b, 16 + i * 16, r, g, bl, 0)                # background gradient
    lights = ((0.5, 0.5, 0.5), (0.0, -0.4, -0.1), (-0.5, -0.5, 0.5))
    for i, (x, y, z) in enumerate(lights):
        struct.pack_into('<ffff', b, 80 + i * 16, x, y, z, 0.0)
    for i in range(3):
        struct.pack_into('<ffff', b, 128 + i * 16, 0.5, 0.5, 0.5, 0.0)
    struct.pack_into('<ffff', b, 176, 0.4, 0.4, 0.4, 0.0)                     # ambient light
    b[192:192 + len(title)] = title
    for off in (260, 324, 388):                                               # list/copy/delete
        name = b'jkps2.ico'
        b[off:off + len(name)] = name
    open(path, 'wb').write(bytes(b))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join('src', 'icon')
    os.makedirs(out, exist_ok=True)
    tex = draw_texture()
    tex.save(os.path.join(out, 'preview.png'))
    write_ico(os.path.join(out, 'jkps2.ico'), tex)
    write_icon_sys(os.path.join(out, 'icon.sys'), 'JUMP KING PS2', 'port de Aroy 24')
    print('icon: icon.sys + jkps2.ico (original character)')


if __name__ == '__main__':
    main()
