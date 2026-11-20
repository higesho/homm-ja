"""LE (DOS/4GW) parser for HEROES2.EXE: objects, pages, fixups."""
import struct, re
class LE:
    def __init__(s, path):
        d = s.d = open(path, 'rb').read()
        s.le = le = d.find(b'LE\0\0', 0x1000)
        g = lambda o, f='<I': struct.unpack_from(f, d, le + o)[0]
        s.psize = g(0x28); s.lastpage = g(0x2c); s.npages = g(0x14)
        s.objtab = le + g(0x40); s.nobj = g(0x44); s.optab = le + g(0x48)
        s.fixpg = le + g(0x68); s.fixrec = le + g(0x6c)
        dp = g(0x80); eo, ep = g(0x18), g(0x1c)
        s.mz = None
        for m in re.finditer(b'MZ', d[:le]):          # bound exe: data pages are relative to an inner MZ
            ob = struct.unpack_from('<6I', d, le + g(0x40) + (eo - 1) * 24)
            fo = m.start() + dp + (ob[3] - 1) * g(0x28) + ep
            if b'WATCOM' in d[fo:fo + 16]: s.mz = m.start()
        s.datapages = s.mz + dp
        s.eip = (g(0x18), g(0x1c)); s.esp = (g(0x20), g(0x24))
        s.objs = []
        for i in range(s.nobj):
            vs, base, fl, pi, np_, _ = struct.unpack_from('<6I', d, s.objtab + i * 24)
            s.objs.append(dict(vs=vs, base=base, fl=fl, pi=pi, np=np_))
        s.pages = []
        for i in range(s.npages):
            b = d[s.optab + i * 4:s.optab + i * 4 + 4]
            s.pages.append(((b[0] << 16) | (b[1] << 8) | b[2], b[3]))
        # memory image per object
        s.mem = []
        for o in s.objs:
            m = bytearray(o['vs'] + 0x1000)
            for k in range(o['np']):
                pn, fl = s.pages[o['pi'] - 1 + k]
                assert fl == 0, fl
                off = s.datapages + (pn - 1) * s.psize
                n = s.lastpage if pn == s.npages else s.psize
                m[k * s.psize:k * s.psize + n] = d[off:off + n]
            s.mem.append(m)
        s.parse_fixups()
    def parse_fixups(s):
        d = s.d
        s.fix = []   # (obj, srcoff_in_obj, stype, flags, tobj, toff, raw_len, pageindex)
        for pi in range(s.npages):
            a = s.fixrec + struct.unpack_from('<I', d, s.fixpg + pi * 4)[0]
            b = s.fixrec + struct.unpack_from('<I', d, s.fixpg + pi * 4 + 4)[0]
            # find object of page
            for oi, o in enumerate(s.objs):
                if o['pi'] - 1 <= pi < o['pi'] - 1 + o['np']: break
            pbase = (pi - (o['pi'] - 1)) * s.psize
            p = a
            while p < b:
                st, fl = d[p], d[p + 1]; p += 2
                assert not (st & 0x20), 'src list'
                srcoff = struct.unpack_from('<h', d, p)[0]; p += 2
                assert (fl & 3) == 0, fl
                if fl & 0x40: tobj = struct.unpack_from('<H', d, p)[0]; p += 2
                else: tobj = d[p]; p += 1
                if (st & 0xf) == 2:  # selector
                    toff = None
                else:
                    if fl & 0x10: toff = struct.unpack_from('<I', d, p)[0]; p += 4
                    else: toff = struct.unpack_from('<H', d, p)[0]; p += 2
                s.fix.append((oi, pbase + srcoff, st, fl, tobj, toff, pi))
    def va(s, oi, off): return s.objs[oi]['base'] + off
    def obj_of(s, va):
        for i, o in enumerate(s.objs):
            if o['base'] <= va < o['base'] + o['vs']: return i
        return None
