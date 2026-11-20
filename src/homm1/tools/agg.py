import struct
def agg(path):
    """HoMM1 AGG: u16 n, n * {u16 hash, u32 offset, u32 size, u32 size}, data, n * 15 bytes of names"""
    a = open(path, 'rb').read()
    n = struct.unpack_from('<H', a, 0)[0]
    names = a[len(a) - n * 15:]
    out = {}
    for i in range(n):
        h, off, sz, sz2 = struct.unpack_from('<HIII', a, 2 + i * 14)
        nm = names[i * 15:i * 15 + 13].split(b'\0')[0].decode()
        out[nm.upper()] = a[off:off + sz]
    return out
