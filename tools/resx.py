"""Lee las cadenas de un ensamblado satelite .NET (.resources incrustado, formato ResourceReader v2).
Se usa con los archivos de idioma de TU instalacion (p. ej. es-ES/LanguageJK.resources.dll)."""
import struct

MAGIC = b'\xce\xca\xef\xbe'


def _7bit(b, p):
    r = s = 0
    while True:
        c = b[p]; p += 1
        r |= (c & 0x7F) << s
        if not c & 0x80:
            return r, p
        s += 7


def read_strings(path):
    data = open(path, 'rb').read()
    out = {}
    start = data.find(MAGIC)
    while start >= 0:
        b = data[start:]
        try:
            _, skip = struct.unpack_from('<Ii', b, 0)
            p = 8 + skip
            ver, n, ntypes = struct.unpack_from('<iii', b, p); p += 12
            for _ in range(ntypes):
                ln, p = _7bit(b, p); p += ln
            p += (8 - (start + p) % 8) % 8 if False else 0
            while b[p:p + 3] == b'PAD' or b[p:p + 1] in (b'P', b'A', b'D'):   # relleno de alineacion
                p += 1
            p += 4 * n                      # hashes
            names = struct.unpack_from(f'<{n}i', b, p); p += 4 * n
            data_off, = struct.unpack_from('<i', b, p); p += 4
            name_sec = p
            for np_ in names:
                q = name_sec + np_
                ln, q = _7bit(b, q)
                name = b[q:q + ln].decode('utf-16-le'); q += ln
                off, = struct.unpack_from('<i', b, q)
                d = data_off + off
                tc, d = _7bit(b, d)
                if tc == 1:                 # String
                    ln, d = _7bit(b, d)
                    out[name] = b[d:d + ln].decode('utf-8')
        except (struct.error, IndexError, UnicodeDecodeError):
            pass
        start = data.find(MAGIC, start + 4)
    return out
