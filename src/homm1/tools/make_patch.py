# build/patch.bin: the data blob of winmm.dll (fonts, translated strings, program patch, map files)
import sys, os, json, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from le import LE
import strings as S
import maptext, mp2, bintext
from agg import agg
EDITOR_BINS = set()
SMALL_ADV, BIG_ADV = 8, 12          # advance of a Japanese char (2 bytes, each measured as sprite 5Fh)
CAP_VAS = [0x22f06, 0x2b2b1, 0x2d5ba, 0x2d887, 0x2d8ce, 0x2dbc9, 0x2dc73, 0x327aa, 0x327f1, 0x32af8, 0x32ba2,
           0x3bae1, 0x3bf93, 0x3cadb, 0x55f59, 0x64360, 0x693d0, 0x7ab58, 0x7f448, 0x7f76e, 0x7f7d8, 0x7f818,
           0x7f904, 0x81f18]          # sentences: first byte "capitalized" by -20h after sprintf (breaks a Japanese lead byte)
def caps():
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    L = S.L; m = L.mem[0]; out = []
    for va in CAP_VAS:
        o = va - L.objs[0]['base']
        i = next(md.disasm(bytes(m[o:o + 16]), va))
        assert i.mnemonic in ('add', 'sub') and i.op_str.endswith(('0xe0', '0x20')), (hex(va), i.mnemonic, i.op_str)
        b = bytes(i.bytes); cmp = 2 if i.size == 7 else i.size
        out.append((o, i.size, b[:cmp]))
    return out

EXE = os.path.join(ROOT, '..', 'HEROES.EXE')
POOL_OFF = (S.L.objs[1]["vs"] + 0x1fff) & ~0xfff   # after the data object (its BSS and stack)
CB_BASE = 0x70
LOG = 0

# ------------------------------------------------------------------ fonts
def bdf(path):
    g = {}; cur = None; rows = None; bbx = None
    for line in open(path, encoding='latin-1'):
        t = line.split()
        if not t: continue
        if t[0] == 'ENCODING': cur = int(t[1])
        elif t[0] == 'BBX': bbx = list(map(int, t[1:5]))
        elif t[0] == 'BITMAP': rows = []
        elif t[0] == 'ENDCHAR':
            if cur is not None and rows is not None: g[cur] = (bbx, rows)
            rows = None; cur = None
        elif rows is not None: rows.append((int(t[0], 16), len(t[0]) * 4))
    return g
def render(g, key, size, desc=2):
    img = [[0] * size for _ in range(size)]
    if key not in g: return None
    (bw, bh, bx, by), rows = g[key]
    top = size - desc - (by + bh)
    for r, (v, bits) in enumerate(rows):
        for x in range(bw):
            if v >> (bits - 1 - x) & 1:
                yy = top + r; xx = bx + x
                if 0 <= yy < size and 0 <= xx < size: img[yy][xx] = 1
    return img
_fonts = {}
def fonts():
    if not _fonts:
        _fonts['misaki'] = bdf(os.path.join(ROOT, 'font', 'misaki_gothic.bdf'))
        _fonts['shn12'] = bdf(os.path.join(ROOT, 'font', 'shnmk12.bdf'))
    return _fonts
def jis(ch):
    b = ch.encode('euc_jp'); return ((b[0] & 0x7f) << 8) | (b[1] & 0x7f)
def glyph_bytes(ch):
    f = fonts()
    s = render(f['misaki'], ord(ch), 8) or [[0] * 8 for _ in range(8)]
    b = render(f['shn12'], jis(ch), 12) or [[0] * 12 for _ in range(12)]
    sm = bytes(sum(s[y][x] << (7 - x) for x in range(8)) for y in range(8))
    bg = b''.join(struct.pack('>H', sum(b[y][x] << (15 - x) for x in range(12))) for y in range(12))
    return sm, bg

NOSTART = set('、。，．・：；？！ー）」』】〕〉》’”ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々〜…‥')
NOEND = set('（「『【〔〈《‘“')
FW = {chr(0xff01 + i): chr(0x21 + i) for i in range(94)}
FW['　'] = '  '

class Enc:
    def __init__(s): s.chars = []; s.idx = {}
    def code(s, ch):
        if ch not in s.idx:
            s.idx[ch] = len(s.chars); s.chars.append(ch)
            assert len(s.chars) <= 4096
        i = s.idx[ch]
        return bytes([0x80 + i // 64, 0xc0 + i % 64])
    def encode(s, text):
        out = bytearray(); prev = None
        for ch in text:
            if ch in FW and ch not in '（）「」！？：' and FW[ch].isalnum(): ch = FW[ch]
            if ord(ch) < 0x80:
                out += ch.encode('latin-1'); prev = ch; continue
            ch.encode('euc_jp')
            out += s.code(ch); prev = ch
        return bytes(out)

# ------------------------------------------------------------------ program
def exe_section(L, d, pool_len):
    recs = []
    ot = L.objtab + 24      # object 2
    vs = (POOL_OFF + pool_len + 0xfff) & ~0xfff
    recs.append((ot, struct.pack('<I', vs)))
    o1 = L.objs[0]; off = 0x742f8
    pn, fl = L.pages[o1['pi'] - 1 + off // 4096]
    fo = L.datapages + (pn - 1) * L.psize + off % 4096
    assert d[fo:fo + 4] == bytes.fromhex('fb83e4fc'), d[fo:fo + 4]
    recs.append((fo, b'\xfe\x38\xff\xff'))
    out = struct.pack('<II', len(d), zlib.crc32(d) & 0xffffffff)
    for o, b in recs:
        out += struct.pack('<II', o, len(b)) + b + b'\0' * ((-len(b)) % 4)
    return out

def name16(n):
    b = n.encode('latin-1'); return b + b'\0' * (16 - len(b))

def load_translations():
    tr = {}
    p = os.path.join(ROOT, 'text', 'ja.json')
    if os.path.exists(p): tr.update(json.load(open(p, encoding='utf-8')))
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p): tr.update(json.load(open(p, encoding='utf-8')))
    return tr

# Hero names: the table at data 0x2150c holds 36 pairs (name with title, short name). When a game starts they are
# strcpy'd into each hero's record: +2 (17 bytes) and +13h (only 9 bytes: 8 letters). A Japanese short name of
# more than 4 chars overflowed into the hero's data (hero not drawn, army 0, crash; seen in campaigns).
# So the short names stay English (never repointed), and the DLL makes the two places that show the short name
# read the long field instead.
HERO_SHORT = {(1, 0x21510 + 8 * k) for k in range(36)}

def main():
    d = open(EXE, 'rb').read()
    L = S.L
    tr = load_translations()
    skip = set()
    p = os.path.join(ROOT, 'text', 'skip_offsets.json')
    if os.path.exists(p): skip = set(int(x, 16) for x in json.load(open(p)))
    enc = Enc()
    pool = bytearray(); recs = []; missing = 0
    p = os.path.join(ROOT, 'text', 'fixes_off.json')
    tr_off = {int(k, 16): v for k, v in json.load(open(p, encoding='utf-8')).items()} if os.path.exists(p) else {}
    bis = os.environ.get('BISECT')          # test only: translate strings with index lo <= i < hi
    lo, hi = (map(int, bis.split(','))) if bis else (0, 1 << 30)
    for idx, (off, s) in enumerate(S.all_strings()):
        if off in skip: continue
        if not (lo <= idx < hi): continue
        t = tr_off.get(off, tr.get(s))
        if t is None or t == s: continue
        sites = [x for x in S.refs.get(off, []) if x not in HERO_SHORT]
        if not sites:
            if not S.refs.get(off): missing += 1
            continue
        if any(oi == 1 and 0x2150c <= so < 0x2162c for oi, so in sites):
            assert len(b_ := enc.encode(t)) <= 16, ('hero name too long for its 17-byte field', s, t)
        b = enc.encode(t) + b'\0'
        recs.append((len(pool), off, [(0x80000000 | so) if oi == 1 else so for oi, so in sites]))
        pool += b
    sect_s = struct.pack('<II', len(recs), len(pool)) + bytes(pool) + b'\0' * ((-len(pool)) % 4)
    for po, oo, sites in recs:
        sect_s += struct.pack('<III', po, oo, len(sites)) + b''.join(struct.pack('<I', x) for x in sites)
    sect_s += struct.pack('<I', 0)
    # maps
    map_sects = []; too_long = []
    for f in ([] if os.environ.get("NO_MAPS") else maptext.map_files()):
        bn = os.path.basename(f).upper()
        try: tt = maptext.texts(f)
        except Exception: continue
        recs_m = []
        for key, kind, en, lim in tt:
            t = tr.get(en)
            if t is None or t == en: continue
            b = enc.encode(t)
            k = key.split(':')
            if os.environ.get('MAP_ONLY') and os.environ['MAP_ONLY'] != k[1]: continue
            if k[0] == 'H':
                off, size = (mp2.NAME_OFF, mp2.NAME_LEN) if k[1] == 'name' else (mp2.DESC_OFF, mp2.DESC_LEN)
                if len(b) > size - 1: too_long.append((bn, kind, t)); continue
                recs_m.append((0, off, size, b))
            elif k[0] == 'F':
                if len(b) > lim: too_long.append((bn, kind, t)); continue
                recs_m.append((1, int(k[1]), int(k[2]) | (lim + 1) << 16, b))
            else:
                recs_m.append((2, int(k[1]), int(k[2]), b))
        if not recs_m: continue
        od = open(f, 'rb').read()
        sec = name16(bn) + struct.pack('<III', len(od), zlib.crc32(od) & 0xffffffff, len(recs_m))
        for kind, a, b2, by in recs_m:
            sec += struct.pack('<IIII', kind, a, b2, len(by)) + by + b'\0' * ((-len(by)) % 4)
        map_sects.append((b'M', sec))
    if too_long:
        with open(os.path.join(ROOT, 'work', 'too_long.txt'), 'w', encoding='utf-8') as fo:
            for x in too_long: fo.write('%s %s %s\n' % x)
    # window layout labels in the AGG archives
    btr = json.load(open(os.path.join(ROOT, 'text', 'bin_ja.json'), encoding='utf-8'))
    agg_sects = []
    for an in ([] if os.environ.get('NO_AGG') else bintext.AGGS):
        ap = os.path.join(ROOT, '..', 'DATA', an)
        ad = open(ap, 'rb').read()
        n = struct.unpack_from('<H', ad, 0)[0]
        names = ad[len(ad) - n * 15:]
        idx = {names[i * 15:i * 15 + 13].split(b'\0')[0].decode().upper(): i for i in range(n)}
        A = agg(ap)
        ents = []
        for k in sorted(A):
            if not (k.endswith('.BIN') or k in ('FONT.ICN', 'SMALFONT.ICN')) or k in EDITOR_BINS: continue
            reps = []
            for off, t in bintext.labels(A[k]):
                j = btr.get(t)
                if not j or j == t: continue
                b = enc.encode(j)
                bz = b + b'\0'
                reps.append(struct.pack('<III', off, len(t) + 1, len(bz)) + bz + b'\0' * ((-len(bz)) % 4))
            if k in ('FONT.ICN', 'SMALFONT.ICN'):     # sprite 5Fh width = half a Japanese char - 1
                hw = (BIG_ADV if k == 'FONT.ICN' else SMALL_ADV) // 2 - 1
                reps.append(struct.pack('<III', 6 + 0x5f * 12 + 4, 0xffffffff, 4) + struct.pack('<HH', hw, 0))
                reps[-1] = struct.pack('<III', 6 + 0x5f * 12 + 4, 0xffffffff, 2) + struct.pack('<H', hw) + b'\0\0'
            if reps: ents.append(struct.pack('<II', idx[k], len(reps)) + b''.join(reps))
        if ents:
            agg_sects.append((b'A', name16(an) + struct.pack('<III', len(ad), zlib.crc32(ad) & 0xffffffff, len(ents)) + b''.join(ents)))
    # fonts
    sm = bytearray(); bg = bytearray()
    for ch in enc.chars:
        a, b = glyph_bytes(ch); sm += a; bg += b
    flags = bytes((1 if c in NOSTART else 0) | (2 if c in NOEND else 0) | (4 if ('ァ' <= c <= 'ヺ' or c == 'ー') else 0) for c in enc.chars)
    sect_f = struct.pack('<I', len(enc.chars)) + bytes(sm) + bytes(bg) + flags
    sect_p = struct.pack('<7I', CB_BASE, LOG, SMALL_ADV, BIG_ADV, 7, 10, POOL_OFF)
    sect_x = name16('HEROES.EXE') + exe_section(L, d, len(pool))
    sect_c = struct.pack('<I', len(CAP_VAS)) + b''.join(struct.pack('<IBB', o, n, len(b)) + b.ljust(6, b'\0') for o, n, b in caps())
    sects = [(b'C', sect_c), (b'P', sect_p), (b'F', sect_f), (b'S', sect_s), (b'X', sect_x)] + map_sects + agg_sects
    out = b'H1JP' + struct.pack('<II', 1, len(sects))
    for t, b in sects:
        b = b + b'\0' * ((-len(b)) % 4)
        out += struct.pack('<II', struct.unpack('<I', t + b'\0' * 3)[0], len(b)) + b
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(out)
    print('strings', len(recs), 'maps', len(map_sects), 'aggs', len(agg_sects), 'too long', len(too_long), 'pool', len(pool), 'glyphs', len(enc.chars), 'no-ref', missing, 'blob', len(out))

if __name__ == '__main__':
    main()
