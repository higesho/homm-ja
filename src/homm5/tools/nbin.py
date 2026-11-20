# Nival binary (HoMM5 bin/*) : entry = u8 field, then length code: even byte b -> len b>>1 ;
# odd byte -> u32 little-endian (including that byte) >> 1. Values are raw bytes or nested entries.
import sys, struct

def entries(d, p=0, end=None):
    end = len(d) if end is None else end
    out = []
    while p < end:
        if p + 2 > end: return None
        f = d[p]; b = d[p + 1]
        if b & 1:
            if p + 5 > end: return None
            n = struct.unpack_from('<I', d, p + 1)[0] >> 1; h = 5
        else:
            n = b >> 1; h = 2
        if p + h + n > end: return None
        out.append((f, p + h, n)); p += h + n
    return out

def dump(d, p=0, end=None, ind=0, depth=8, maxn=40):
    es = entries(d, p, end)
    if es is None: return False
    for k, (f, s, n) in enumerate(es):
        if k >= maxn: print(' ' * ind + '... %d more' % (len(es) - k)); break
        sub = entries(d, s, s + n) if n > 4 and depth > 0 else None
        if sub:
            print(' ' * ind + '%02x [%d] {' % (f, n))
            dump(d, s, s + n, ind + 2, depth - 1, maxn)
            print(' ' * ind + '}')
        else:
            v = d[s:s + n]
            iv = struct.unpack('<i', v)[0] if n == 4 else (struct.unpack('<h', v)[0] if n == 2 else None)
            print(' ' * ind + '%02x [%d] %s %s' % (f, n, v[:24].hex(' '), '' if iv is None else '= %d' % iv))
    return True

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    dump(d, maxn=int(sys.argv[2]) if len(sys.argv) > 2 else 40)
