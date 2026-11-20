# table.* / strings.* files: u32 rows; row = u16 ncols, ncols * (u16 len + bytes); strings.* have extra data after
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import h4r
GAME = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
def parse(b):
    p = 0; n = struct.unpack_from('<I', b, p)[0]; p += 4; rows = []
    for i in range(n):
        nc = struct.unpack_from('<H', b, p)[0]; p += 2; r = []
        for j in range(nc):
            l = struct.unpack_from('<H', b, p)[0]; p += 2; r.append(b[p:p + l].decode('latin-1')); p += l
        rows.append(r)
    return rows
def files():
    for a in ['text', 'updates', 'storm_override', 'x2_override']:
        d, E = h4r.entries(os.path.join(GAME, 'data', a + '.h4r'))
        for e in E:
            if e['name'].split('.')[0] in ('table', 'strings'): yield a, e['name'], parse(h4r.read(d, e))
