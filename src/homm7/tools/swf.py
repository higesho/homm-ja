import struct, sys
def tags(b):
    # header: sig(3) ver(1) len(4) rect, framerate u16, framecount u16
    nb = b[8] >> 3
    rbits = 5 + nb * 4
    p = 8 + (rbits + 7) // 8 + 4
    hdr = p
    while p < len(b):
        cl, = struct.unpack_from('<H', b, p); p += 2
        code, ln = cl >> 6, cl & 0x3f
        if ln == 0x3f:
            ln, = struct.unpack_from('<I', b, p); p += 4
        yield code, p, ln
        p += ln
        if code == 0: break
def font_info(b, p, ln, code):
    fid, flags = struct.unpack_from('<HB', b, p); q = p + 3
    lang = b[q]; q += 1
    nl = b[q]; q += 1
    name = b[q:q + nl].decode('utf-8', 'replace').rstrip('\0'); q += nl
    ng, = struct.unpack_from('<H', b, q); q += 2
    wide_off = flags & 0x08; wide_codes = flags & 0x04
    offs = q
    osz = 4 if wide_off else 2
    if ng:
        codetab_off, = struct.unpack_from('<I' if wide_off else '<H', b, offs + ng * osz)
        ct = offs + codetab_off
        codes = struct.unpack_from('<%dH' % ng, b, ct) if wide_codes else tuple(b[ct:ct + ng])
    else:
        codes = ()
    return dict(id=fid, flags=flags, name=name, n=ng, codes=codes)
if __name__ == '__main__':
    b = open(sys.argv[1], 'rb').read()
    from collections import Counter
    c = Counter()
    for code, p, ln in tags(b):
        c[code] += 1
        if code in (48, 62, 75):
            f = font_info(b, p, ln, code)
            cj = sum(1 for x in f['codes'] if 0x4e00 <= x <= 0x9fff); ka = sum(1 for x in f['codes'] if 0x3040 <= x <= 0x30ff)
            print('tag', code, 'id', f['id'], 'flags %02x' % f['flags'], repr(f['name']), 'glyphs', f['n'], 'kanji', cj, 'kana', ka)
        elif code == 56:  # ExportAssets
            n, = struct.unpack_from('<H', b, p); q = p + 2; L = []
            for _ in range(n):
                i, = struct.unpack_from('<H', b, q); q += 2; e = b.index(b'\0', q); L.append((i, b[q:e].decode('utf-8', 'replace'))); q = e + 1
            print('export', L[:20], len(L))
        elif code == 88:  # DefineFontName
            fid, = struct.unpack_from('<H', b, p); e = b.index(b'\0', p + 2)
            print('fontname', fid, b[p + 2:e].decode('utf-8', 'replace'))
    print(sorted(c.items()))
