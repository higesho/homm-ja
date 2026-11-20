# Read Heroes VI .orc archives: a ZIP whose header records are each encrypted (modified ZipCrypto,
# keys seeded with the record's file offset); file data is plain (stored/deflate).
import os, sys, struct, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orc

class Entry:
    __slots__ = ('name', 'method', 'csize', 'usize', 'lho', 'crc', 'arc')

class Orc:
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'rb')
        n = os.path.getsize(path)
        self.f.seek(n - 22); eo = orc.Keys(n - 22).decrypt(self.f.read(22))
        assert eo[:4] == b'PK\x05\x06', path
        cnt, cdsize, cdoff = struct.unpack_from('<HII', eo, 10)
        self.f.seek(cdoff); cd = orc.Keys(cdoff).decrypt(self.f.read(cdsize)) if cdsize < 4_000_000 else self._dec_big(cdoff, cdsize)
        self.entries = []; p = 0
        for _ in range(cnt):
            assert cd[p:p + 4] == b'PK\x01\x02'
            method, = struct.unpack_from('<H', cd, p + 10)
            crc, cs, us, nl, xl, cl = struct.unpack_from('<IIIHHH', cd, p + 16)
            lho, = struct.unpack_from('<I', cd, p + 42)
            e = Entry(); e.name = cd[p + 46:p + 46 + nl].decode('utf-8', 'replace'); e.method = method
            e.csize, e.usize, e.lho, e.crc, e.arc = cs, us, lho, crc, self
            self.entries.append(e); p += 46 + nl + xl + cl

    def _dec_big(self, off, size):
        return orc.Keys(off).decrypt(self.f.read(size))

    def read(self, e):
        self.f.seek(e.lho); h = orc.Keys(e.lho).decrypt(self.f.read(30))
        assert h[:4] == b'PK\x03\x04', (self.path, e.name)
        nl, xl = struct.unpack_from('<HH', h, 26)
        self.f.seek(e.lho + 30 + nl + xl); d = self.f.read(e.csize)
        if e.method == 8: d = zlib.decompressobj(-15).decompress(d)
        elif e.method != 0: raise ValueError('method %d' % e.method)
        return d

GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

def all_orcs():
    import glob
    return sorted(glob.glob(os.path.join(GAME, '*.orc')))

if __name__ == '__main__':
    for p in (sys.argv[1:] or all_orcs()):
        o = Orc(p)
        tot = sum(e.usize for e in o.entries)
        print('==', os.path.basename(p), len(o.entries), 'files', tot)
        for e in o.entries[:int(os.environ.get('N', '8'))]: print('   ', e.name, e.usize, e.method)
