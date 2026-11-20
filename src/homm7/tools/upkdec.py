# Decompress a UE3 package (v868, H7 tag) whose PKG_StoreCompressed flag is set -> plain package bytes
import struct, sys, zlib, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lzo1x
def header_chunks(d):
    p = 8 + 4
    n, = struct.unpack_from('<i', d, p); p += 4
    p += n if n >= 0 else -n * 2
    flags, = struct.unpack_from('<I', d, p); p += 4
    p += 4 * 7            # names, exports, imports, depends
    p += 4 * 4            # importexportguids, importguids, exportguids, thumbnail
    p += 16
    gc, = struct.unpack_from('<i', d, p); p += 4 + gc * 12
    p += 8                # engine, cooker
    cflags, nch = struct.unpack_from('<II', d, p); p += 8
    ch = [struct.unpack_from('<iiii', d, p + 16 * i) for i in range(nch)]
    return flags, cflags, ch, p, p + 16 * nch
def decompress(d):
    flags, cflags, ch, cfpos, after = header_chunks(d)
    if not ch: return d
    total = max(u + us for u, us, c, cs in ch)
    out = bytearray(total)
    out[:ch[0][0]] = d[:ch[0][0]]
    for uoff, usize, coff, csize in ch:
        tag, bs, cs, us = struct.unpack_from('<IIII', d, coff)
        q = coff + 16; blocks = []
        n = (us + bs - 1) // bs
        for _ in range(n):
            blocks.append(struct.unpack_from('<II', d, q)); q += 8
        o = uoff
        for bcs, bus in blocks:
            if cflags & 1: data = zlib.decompress(d[q:q + bcs])
            elif cflags & 2: data = lzo1x.decompress(d[q:q + bcs], bus)
            else: raise SystemExit('compression %d not supported' % cflags)
            out[o:o + bus] = data; o += bus; q += bcs
    # clear compressed flag and chunk count
    struct.pack_into('<I', out, 8 + 4 + 4 + (struct.unpack_from('<i', d, 12)[0]), flags & ~0x02000000)
    struct.pack_into('<II', out, cfpos, 0, 0)
    return bytes(out)
if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    f, cf, ch, a, b = header_chunks(d)
    print('flags %08x compression %d chunks %d' % (f, cf, len(ch)))
    if len(sys.argv) > 2:
        open(sys.argv[2], 'wb').write(decompress(d))
