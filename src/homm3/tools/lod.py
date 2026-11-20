# LOD archive reader: python tools/lod.py list FILE | extract FILE OUTDIR [pattern]
import sys, struct, zlib, os, fnmatch
def read_dir(path):
    f = open(path, 'rb'); h = f.read(92)
    assert h[:4] == b'LOD\0', path
    n = struct.unpack_from('<I', h, 8)[0]
    ents = []
    for i in range(n):
        e = f.read(32)
        name = e[:16].split(b'\0')[0].decode('latin-1')
        off, size, typ, csize = struct.unpack_from('<4I', e, 16)
        ents.append((name, off, size, typ, csize))
    return f, struct.unpack_from('<I', h, 4)[0], ents
def get(f, e):
    name, off, size, typ, csize = e
    f.seek(off)
    if csize: return zlib.decompress(f.read(csize))
    return f.read(size)
if __name__ == '__main__':
    f, ver, ents = read_dir(sys.argv[2])
    if sys.argv[1] == 'list':
        print('ver', ver, 'count', len(ents), 'first off', min(e[1] for e in ents))
        for e in ents: print(*e)
    else:
        out = sys.argv[3]; pat = sys.argv[4] if len(sys.argv) > 4 else '*'
        os.makedirs(out, exist_ok=True)
        for e in ents:
            if fnmatch.fnmatch(e[0].lower(), pat.lower()):
                open(os.path.join(out, e[0]), 'wb').write(get(f, e))
