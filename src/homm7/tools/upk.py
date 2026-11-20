# Minimal UE3 (v868, H7 tag) package reader
import struct, sys

class Pkg:
    def __init__(self, path):
        self.path = path
        self.d = d = open(path, 'rb').read()
        self.tag, self.ver, self.lic = struct.unpack_from('<IHH', d, 0)
        p = 8
        self.hdrsize, = struct.unpack_from('<i', d, p); p += 4
        n, = struct.unpack_from('<i', d, p); p += 4
        p += n if n >= 0 else -n * 2
        (self.flags, self.ncount, self.noff, self.ecount, self.eoff, self.icount, self.ioff,
         self.depoff) = struct.unpack_from('<8I', d, p)
        self.names = []
        p = self.noff
        for _ in range(self.ncount):
            n, = struct.unpack_from('<i', d, p); p += 4
            if n >= 0:
                s = d[p:p + n - 1].decode('latin1'); p += n
            else:
                s = d[p:p - n * 2 - 2].decode('utf-16le'); p += -n * 2
            p += 8
            self.names.append(s)
        self.imports = []
        p = self.ioff
        for _ in range(self.icount):
            cp, cpn, cn, cnn, outer, on, onn = struct.unpack_from('<iiiiiii', d, p); p += 28
            self.imports.append(dict(cpkg=self.nm(cp, cpn), cls=self.nm(cn, cnn), outer=outer, name=self.nm(on, onn)))
        self.exports = []
        p = self.eoff
        for i in range(self.ecount):
            st = p
            cls, sup, outer, on, onn, arch = struct.unpack_from('<iiiiii', d, p); p += 24
            oflags, = struct.unpack_from('<Q', d, p); p += 8
            size, off, eflags, netc = struct.unpack_from('<iiIi', d, p); p += 16
            p += netc * 4 + 16 + 4
            self.exports.append(dict(idx=i, cls=cls, sup=sup, outer=outer, name=self.nm(on, onn), arch=arch,
                                     oflags=oflags, size=size, off=off, hdr=st, hdrend=p))

    def nm(self, i, n=0):
        s = self.names[i]
        return s if n == 0 else '%s_%d' % (s, n - 1)

    def objname(self, ref):
        if ref == 0: return 'None'
        if ref > 0: return self.exports[ref - 1]['name']
        return self.imports[-ref - 1]['name']

    def clsname(self, e):
        return 'Class' if e['cls'] == 0 else self.objname(e['cls'])

    def opath(self, ref):
        parts = []
        while ref:
            if ref > 0:
                e = self.exports[ref - 1]; parts.append(e['name']); ref = e['outer']
            else:
                im = self.imports[-ref - 1]; parts.append(im['name']); ref = im['outer']
        return '.'.join(reversed(parts))

if __name__ == '__main__':
    k = Pkg(sys.argv[1])
    print('ver', k.ver, k.lic, 'names', k.ncount, 'exports', k.ecount, 'imports', k.icount)
    flt = sys.argv[2] if len(sys.argv) > 2 else ''
    for e in k.exports:
        s = '%4d %-22s %-60s off=%d size=%d' % (e['idx'] + 1, k.clsname(e), k.opath(e['idx'] + 1), e['off'], e['size'])
        if flt.lower() in s.lower(): print(s)
