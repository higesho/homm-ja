# HoMM5 font bin (bin/fonts/<uid>) reader/writer
import sys, struct, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nbin
def _find(d, es, f): return [e for e in es if e[0] == f][0]
def parse(d):
    top = nbin.entries(d)
    f2 = _find(d, top, 2); l1 = nbin.entries(d, f2[1], f2[1] + f2[2]); g = _find(d, l1, 1)
    l2 = nbin.entries(d, g[1], g[1] + g[2]); body = _find(d, l2, 1)
    fs = nbin.entries(d, body[1], body[1] + body[2])
    hdr = {}
    for f, s, n in fs:
        if n <= 4: hdr[f] = int.from_bytes(d[s:s + n], 'little', signed=True)
    maps = [e for e in fs if e[0] in (1, 2) and e[2] > 4]
    m = maps[0]; me = nbin.entries(d, m[1], m[1] + m[2])
    keys = [struct.unpack_from('<H', d, s)[0] for f, s, n in me if f == 1]
    vals = [struct.unpack_from('<7i', d, s) for f, s, n in me if f == 2]
    return hdr, dict(zip(keys, vals))
if __name__ == '__main__':
    import zipfile, re
    z = zipfile.ZipFile(sys.argv[1])
    for n in z.namelist():
        if n.endswith('(Font).xdb'):
            x = z.read(n).decode(); uid = re.search('<uid>(.*?)</uid>', x).group(1)
            b = [k for k in z.namelist() if k.lower() == ('bin/fonts/' + uid).lower()]
            if not b: continue
            hdr, g = parse(z.read(b[0]))
            print(n.split('/')[-1], re.search('<Height>(.*?)<', x).group(1), {hex(k): v for k, v in hdr.items()}, 'A', g.get(65), 'g', g.get(103))
