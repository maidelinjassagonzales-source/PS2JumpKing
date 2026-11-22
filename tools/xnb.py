"""Lector minimo de XNB (XNA 4 / MonoGame, sin comprimir): Texture2D y SoundEffect.
Lee los archivos de TU copia del juego; no guarda nada en el repo."""
import struct


class XnbError(Exception):
    pass


def _read_7bit(buf, pos):
    result = shift = 0
    while True:
        b = buf[pos]; pos += 1
        result |= (b & 0x7F) << shift
        if not b & 0x80:
            return result, pos
        shift += 7


def _read_string(buf, pos):
    n, pos = _read_7bit(buf, pos)
    return buf[pos:pos + n].decode('utf-8'), pos + n


def read(path):
    """Devuelve (tipo, objeto). tipo: 'texture' -> dict(w,h,fmt,rgba) | 'sound' -> dict(fmt,data,...)"""
    buf = open(path, 'rb').read()
    if buf[:3] != b'XNB':
        raise XnbError(f'{path}: no es XNB')
    version, flags = buf[4], buf[5]
    if flags & 0xC0:
        raise XnbError(f'{path}: XNB comprimido (no soportado todavia)')
    pos = 10
    nreaders, pos = _read_7bit(buf, pos)
    readers = []
    for _ in range(nreaders):
        name, pos = _read_string(buf, pos)
        pos += 4
        readers.append(name)
    _, pos = _read_7bit(buf, pos)          # recursos compartidos
    idx, pos = _read_7bit(buf, pos)
    reader = readers[idx - 1]
    if 'Texture2DReader' in reader:
        fmt, w, h, mips = struct.unpack_from('<iIII', buf, pos); pos += 16
        size, = struct.unpack_from('<I', buf, pos); pos += 4
        if fmt != 0:
            raise XnbError(f'{path}: formato de superficie {fmt} no soportado')
        return 'texture', dict(w=w, h=h, rgba=buf[pos:pos + size])
    if 'SoundEffectReader' in reader:
        fsz, = struct.unpack_from('<I', buf, pos); pos += 4
        tag, ch, rate, _, align, bits = struct.unpack_from('<HHIIHH', buf, pos); pos += fsz
        dsz, = struct.unpack_from('<I', buf, pos); pos += 4
        data = buf[pos:pos + dsz]; pos += dsz
        loop_start, loop_len, dur = struct.unpack_from('<iii', buf, pos)
        return 'sound', dict(tag=tag, channels=ch, rate=rate, bits=bits, data=data,
                             loop_start=loop_start, loop_len=loop_len, ms=dur)
    raise XnbError(f'{path}: lector {reader} no soportado')
