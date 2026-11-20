# HoMM4 .h4r archive: "H4R\x05", u32 dir offset; dir: u32 n, n * {u32 off, u32 csize, u32 usize, u32 time,
#   u16 nl, name, u16 pl, path, u16 al, alias (al > 0: same data as that entry), u32 type}; each file is gzip at off
import struct, zlib, os, sys
def entries(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'H4R\x05'
    p = struct.unpack_from('<I', d, 4)[0]
    n = struct.unpack_from('<I', d, p)[0]; p += 4; out = []
    for i in range(n):
        off, cs, us, tm = struct.unpack_from('<4I', d, p); p += 16
        nl = struct.unpack_from('<H', d, p)[0]; p += 2; name = d[p:p + nl].decode('latin-1'); p += nl
        pl = struct.unpack_from('<H', d, p)[0]; p += 2; pth = d[p:p + pl].decode('latin-1'); p += pl
        al = struct.unpack_from('<H', d, p)[0]; p += 2; alias = d[p:p + al].decode('latin-1'); p += al
        typ = struct.unpack_from('<I', d, p)[0]; p += 4
        out.append(dict(name=name, off=off, cs=cs, us=us, type=typ, alias=alias, path=pth))
    return d, out
def read(d, e):
    return zlib.decompress(d[e['off']:e['off'] + e['cs']], 31) if e['cs'] != e['us'] or d[e['off']:e['off'] + 2] == b'\x1f\x8b' else d[e['off']:e['off'] + e['us']]
if __name__ == '__main__':
    d, E = entries(sys.argv[1])
    for e in E: print(e['type'], e['us'], e['name'])
