# Write a Heroes VI .orc: a ZIP (deflate) whose local headers, central directory and end record are each
# encrypted with the modified ZipCrypto keyed by the record's file offset (see orc.py / orcfs.py).
import io, os, sys, struct, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orc

def enc(seed, data):
    k = orc.Keys(seed); out = bytearray(len(data))
    for i, p in enumerate(data):
        t = (k.k2 | 2) & 0xffff
        out[i] = p ^ ((t * (t ^ 1)) >> 8 & 0xff)
        k.update(p)
    return bytes(out)

def pack(files, path, stamp=(2026, 1, 1, 0, 0, 0)):
    """files: {archive name: bytes}"""
    bio = io.BytesIO()
    with zipfile.ZipFile(bio, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in sorted(files):
            zi = zipfile.ZipInfo(name, date_time=stamp); zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi, files[name])
    d = bytearray(bio.getvalue())
    z = zipfile.ZipFile(io.BytesIO(bytes(d)))
    for i in z.infolist():
        o = i.header_offset
        nl, xl = struct.unpack_from('<HH', d, o + 26)
        n = 30 + nl + xl
        d[o:o + n] = enc(o, bytes(d[o:o + n]))
    eo = len(d) - 22
    assert d[eo:eo + 4] == b'PK\x05\x06'
    cnt, cds, cdo = struct.unpack_from('<HII', d, eo + 10)
    d[cdo:cdo + cds] = enc(cdo, bytes(d[cdo:cdo + cds]))
    d[eo:eo + 22] = enc(eo, bytes(d[eo:eo + 22]))
    open(path, 'wb').write(d)
    return len(d)
