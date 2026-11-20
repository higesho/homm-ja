import struct
def agg(path):
    a = open(path, 'rb').read()
    n = struct.unpack_from('<H', a, 0)[0]
    names = a[len(a) - n * 15:]
    out = {}
    for i in range(n):
        h, off, sz = struct.unpack_from('<III', a, 2 + i * 12)
        nm = names[i * 15:i * 15 + 13].split(b'\0')[0].decode()
        out[nm.upper()] = a[off:off + sz]
    return out
