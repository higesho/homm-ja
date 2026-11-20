# all scenario/campaign files: maps/*.h4c, maps/Contest Maps/*.h4c and game_maps.* in the archives.
# file = "H4CAMPAIGN" u32 + gzip members; strings are u16 length + bytes (read_string 0x879420)
import os, sys, zlib, struct, re, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import h4r
GAME = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
def files():
    for p in sorted(glob.glob(os.path.join(GAME, 'maps', '*.h4c')) + glob.glob(os.path.join(GAME, 'maps', 'Contest Maps', '*.h4c'))):
        yield os.path.relpath(p, GAME), open(p, 'rb').read()
    for a in ['heroes4', 'storm', 'x2', 'updates']:
        d, E = h4r.entries(os.path.join(GAME, 'data', a + '.h4r'))
        for e in E:
            if e['name'].startswith('game_maps.'): yield a + ':' + e['name'], h4r.read(d, e)
def members(b):
    out = []; p = b.find(b'\x1f\x8b\x08')
    while p >= 0:
        z = zlib.decompressobj(31)
        try: m = z.decompress(b[p:])
        except zlib.error: p = b.find(b'\x1f\x8b\x08', p + 1); continue
        out.append(m); used = len(b) - p - len(z.unused_data); p = b.find(b'\x1f\x8b\x08', p + used)
    return out
def scan(m):
    out = []; p = 0; n = len(m)
    while p < n - 2:
        l = m[p] | m[p + 1] << 8
        if 1 <= l < 8000 and p + 2 + l <= n:
            s = m[p + 2:p + 2 + l]
            if all(32 <= c < 127 or c in (9, 10, 13) or c >= 0x91 for c in s) and re.search(rb'[A-Za-z]{2}', s):
                out.append((p, s.decode('latin-1'))); p += 2 + l; continue
        p += 1
    return out
if __name__ == '__main__':
    for name, b in files():
        ms = members(b)
        print(name, len(b), b[:14], [len(m) for m in ms])
