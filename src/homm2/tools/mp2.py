# MP2/MX2 (scenario) and H2C/HXC (campaign map) files: header name/description and the trailing info blocks
import struct, os, sys, re
HDR = 428                      # map data starts here; width/height u32 at 420/424
NAME_OFF, NAME_LEN = 0x3a, 16
DESC_OFF, DESC_LEN = 0x76, 143

class Map:
    def __init__(s, path):
        d = s.d = open(path, 'rb').read()
        s.w, s.h = struct.unpack_from('<II', d, 420)
        p = HDR + s.w * s.h * 20
        na, = struct.unpack_from('<I', d, p); p += 4 + na * 15
        p += 72 * 3 + 144 * 3 + 1
        # block count: byte pairs until 0,0
        cnt = 0
        while True:
            l, h = d[p], d[p + 1]; p += 2
            if l == 0 and h == 0: break
            cnt = 256 * h + l - 1
        s.blocks_at = p
        s.blocks = []
        for i in range(cnt):
            n, = struct.unpack_from('<H', d, p); p += 2
            s.blocks.append(bytearray(d[p:p + n])); p += n
        s.tail = d[p:]
    def cstr(s, off, n):
        b = s.d[off:off + n]; return b.split(b'\0')[0].decode('latin-1')
    def name(s): return s.cstr(NAME_OFF, NAME_LEN)
    def desc(s): return s.cstr(DESC_OFF, DESC_LEN)

def block_text(b):
    """(offset, text) of the message of an info block, or None. Known layouts (fheroes2):
       sign/bottle 9, event 50, sphinx 138 (answers at 10, 8 x 13), daily event 49, rumour 8;
       castle (70 bytes) name at 0x1d? / hero (76 bytes) name: fixed fields, handled separately"""
    n = len(b)
    if n < 9 or b[-1] != 0: return None
    for off in (9, 50, 138, 49, 8):
        if off < n - 1:
            t = bytes(b[off:-1])
            if t and b'\0' not in t and all(32 <= c < 127 or c in (10, 13) for c in t) and (off == 0 or b[off - 1] in (0, 1) or off in (8, 9)):
                return off, t.decode('latin-1')
    return None

if __name__ == '__main__':
    m = Map(sys.argv[1])
    print(m.w, m.h, repr(m.name()), repr(m.desc()), len(m.blocks), 'tail', len(m.tail))
    for i, b in enumerate(m.blocks):
        t = block_text(b)
        print(i, len(b), b[:12].hex(), repr(t)[:120] if t else '')
