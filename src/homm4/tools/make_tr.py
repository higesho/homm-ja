# translations ({English: Japanese}) -> build/tr.bin and build/ts.bin
#  tr.bin: "H4TR", u32 n, n * {u64 fnv1a64(English), u32 off, u32 len | 0x80000000 if the string occurs in maps}, EUC-JP blob
#  ts.bin: "H4TS", u32 n, n * {u64 fnv1a64(signature of the first two rows), u32 first row, u32 0, u64 column mask}
import json, os, struct, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
def fnv(b):
    h = 0xcbf29ce484222325
    for c in b: h = ((h ^ c) * 0x100000001b3) & 0xffffffffffffffff
    return h
NORM = {'—': '―', '–': '－', '～': '〜', '−': '－', '·': '・', '’': "'", '‘': "'",
        '“': '「', '”': '」', ' ': ' ', '…': '…', '•': '・', 'é': 'e', 'è': 'e', 'à': 'a',
        'ï': 'i', 'ö': 'o', 'ü': 'u', 'ä': 'a', 'â': 'a', 'ê': 'e', 'ô': 'o', 'ç': 'c', '‥': '‥'}
for _c in range(0x80, 0xa0):   # Windows-1252 punctuation that came through as C1 controls
    try: NORM.setdefault(chr(_c), bytes([_c]).decode('cp1252'))
    except UnicodeDecodeError: pass
def norm(s):
    s = ''.join(NORM.get(c, c) for c in s)
    return ''.join(NORM.get(c, c) for c in s)
RAW = {'\u00a9': b'\xa9', '\u00ae': b'\xae', '\u2122': b'\x99'}   # the game fonts have these Latin-1 / cp1252 glyphs
def enc(ja):
    out = bytearray()
    for c in norm(ja):
        if c in RAW: out += RAW[c]; continue
        try: out += c.encode('euc_jp')
        except UnicodeEncodeError: out += b'?'; print('  unencodable', repr(c), 'in', ja[:40])
    return bytes(out)
def build(d, mapset, specs):
    ents = {}
    for en, ja in d.items():
        if not ja or ja == en: continue
        k = fnv(en.encode('latin-1'))
        ents[k] = (enc(ja), en in mapset)
    keys = sorted(ents); blob = bytearray(); tab = bytearray()
    for k in keys:
        b, m = ents[k]
        tab += struct.pack('<QII', k, len(blob), len(b) | (0x80000000 if m else 0)); blob += b + b'\0'
    open(os.path.join(ROOT, 'build', 'tr.bin'), 'wb').write(b'H4TR' + struct.pack('<I', len(keys)) + tab + blob)
    ts = bytearray(b'H4TS' + struct.pack('<I', len(specs)))
    for s in specs:
        mask = 0
        for c in s['cols']: mask |= 1 << c
        ts += struct.pack('<QIIQ', fnv(s['sig'].encode('latin-1')), s['first'], 0, mask)
    open(os.path.join(ROOT, 'build', 'ts.bin'), 'wb').write(bytes(ts))
    print(len(keys), 'strings', len(blob), 'bytes;', len(specs), 'tables')
if __name__ == '__main__':
    d = {}
    for fn in sys.argv[1:]: d.update(json.load(open(fn, encoding='utf-8')))
    mapset = set(json.load(open(os.path.join(ROOT, 'text', 'src', 'mapset.json'), encoding='utf-8')))
    specs = json.load(open(os.path.join(ROOT, 'text', 'src', 'tables.json'), encoding='utf-8'))
    build(d, mapset, specs)
