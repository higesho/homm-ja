# Minimal writer for Heroes VII (UE3 v868, licensee 4, "7HMM" tag) cooked packages holding our own objects.
import struct, os, uuid, zlib

TAG = 0x4D4D4837

def fstr(s):
    b = s.encode('latin1') + b'\0'
    return struct.pack('<i', len(b)) + b

class Package:
    def __init__(self, guid=None):
        self.names = []; self.nidx = {}
        self.imports = []          # (cpkg, cls, outer_ref, name)
        self.exports = []          # dict(cls, outer, name, oflags, build)
        self.guid = guid or uuid.uuid4().bytes
        self.name('None')

    def name(self, s):
        if s not in self.nidx:
            self.nidx[s] = len(self.names); self.names.append(s)
        return self.nidx[s]

    def nref(self, s):
        return struct.pack('<ii', self.name(s), 0)

    def imp(self, cpkg, cls, outer, name):
        for k, i in enumerate(self.imports):
            if i == (cpkg, cls, outer, name): return -(k + 1)
        self.name(cpkg); self.name(cls); self.name(name)
        self.imports.append((cpkg, cls, outer, name))
        return -len(self.imports)

    def class_ref(self, pkg, cls):
        p = self.imp('Core', 'Package', 0, pkg)
        return self.imp('Core', 'Class', p, cls)

    def exp(self, cls_ref, name, build, outer=0, oflags=0x000F000400000000):
        """build(pkg, file_offset_of_object_data, export_index0) -> bytes"""
        self.name(name)
        self.exports.append(dict(cls=cls_ref, outer=outer, name=name, oflags=oflags, build=build))
        return len(self.exports)

    # ---- property helpers ----
    def prop(self, name, typ, data, extra=b''):
        return self.nref(name) + self.nref(typ) + struct.pack('<ii', len(data), 0) + extra + data
    def p_int(self, name, v): return self.prop(name, 'IntProperty', struct.pack('<i', v))
    def p_float(self, name, v): return self.prop(name, 'FloatProperty', struct.pack('<f', v))
    def p_bool(self, name, v):
        return self.nref(name) + self.nref('BoolProperty') + struct.pack('<ii', 0, 0) + bytes([1 if v else 0])
    def p_byte_enum(self, name, enum, val):
        return self.prop(name, 'ByteProperty', self.nref(val), self.nref(enum))
    def p_str(self, name, s): return self.prop(name, 'StrProperty', fstr(s))
    def p_struct(self, name, sname, data): return self.prop(name, 'StructProperty', data, self.nref(sname))
    def p_array(self, name, count, data): return self.prop(name, 'ArrayProperty', struct.pack('<i', count) + data)
    def none(self): return self.nref('None')

    def save(self, path, flags=0x20000009, source=None):
        # names may still grow while objects are built: build objects once to collect names, then for real
        dummy = [e['build'](self, 0, k) for k, e in enumerate(self.exports)]
        nc, ic, ec = len(self.names), len(self.imports), len(self.exports)
        sumlen = 4 + 4 + 4 + len(fstr('None')) + 4 + 4 * 7 + 4 * 4 + 16 + 4 + 12 + 4 + 4 + 4 + 4 + 4 + 4 + 4
        noff = sumlen
        names = b''.join(fstr(n) + struct.pack('<Q', 0x0007001000000000) for n in self.names)
        ioff = noff + len(names)
        eoff = ioff + 28 * ic
        doff = eoff + 68 * ec
        thumb = doff + 4 * ec
        hdr = thumb + 4
        # object data
        datas = []; pos = hdr
        for k, e in enumerate(self.exports):
            d = e['build'](self, pos, k)
            datas.append((pos, d)); pos += len(d)
        assert len(self.names) == nc, 'names changed during build'
        out = bytearray()
        out += struct.pack('<IHHi', TAG, 868, 4, hdr) + fstr('None')
        out += struct.pack('<I', flags)
        out += struct.pack('<iiiiiii', nc, noff, ec, eoff, ic, ioff, doff)
        out += struct.pack('<iiii', thumb, 0, 0, thumb)
        out += self.guid
        out += struct.pack('<i', 1) + struct.pack('<iii', ec, nc, ec)
        out += struct.pack('<ii', 12161, 136)
        out += struct.pack('<ii', 0, 0)                     # compression flags, chunks
        out += struct.pack('<I', source if source is not None else (zlib.crc32(self.guid) & 0xffffffff))
        out += struct.pack('<i', 0)                         # additional packages to cook
        out += struct.pack('<i', 0)                         # texture allocations
        assert len(out) == noff, (len(out), noff)
        out += names
        for cpkg, cls, outer, name in self.imports:
            out += self.nref(cpkg) + self.nref(cls) + struct.pack('<i', outer) + self.nref(name)
        for (pos, d), e in zip(datas, self.exports):
            out += struct.pack('<iii', e['cls'], 0, e['outer']) + self.nref(e['name']) + struct.pack('<i', 0)
            out += struct.pack('<Q', e['oflags']) + struct.pack('<ii', len(d), pos)
            out += struct.pack('<Ii', 0, 0) + b'\0' * 16 + struct.pack('<I', 0)
        out += b'\0' * (4 * ec)
        out += struct.pack('<i', 0)
        assert len(out) == hdr
        for pos, d in datas: out += d
        open(path, 'wb').write(out)
        return len(out)
