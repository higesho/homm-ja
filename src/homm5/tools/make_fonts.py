# Japanese fonts for HoMM5: for every UI font (UI/Fonts/*.(Font).xdb) build
#   bin/fonts/<uid>                 glyph table (Nival binary, same layout as the original)
#   UI/Fonts/<name>.(Texture).xdb   texture description with the new atlas size
#   UI/Fonts/<name>.(Texture).dds   A8R8G8B8 atlas (premultiplied white), every glyph drawn from Noto (OFL)
# Latin letters are drawn from the same Noto fonts, so no original game data is reused.
#   python tools/make_fonts.py <texts.pak> <charset.txt> <outdir>
import os, sys, re, struct, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nbin, h5font
from PIL import Image, ImageDraw, ImageFont

SERIF = r'C:\Windows\Fonts\NotoSerifJP-VF.ttf'
SANS = r'C:\Windows\Fonts\NotoSansJP-VF.ttf'
PRIMES = [53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157]

def enc_len(n):
    return bytes([n << 1]) if n < 128 else struct.pack('<I', (n << 1) | 1)
def ent(f, b):
    return bytes([f]) + enc_len(len(b)) + b

def rebuild(d, newmap):
    """original bin bytes -> same bytes with the glyph map (body field 01) replaced"""
    def sub(lo, hi, path):
        out = bytearray(); p = lo
        for f, s, n in nbin.entries(d, lo, hi):
            if path and f == path[0][0] and n == path[0][1]:
                inner = sub(s, s + n, path[1:]) if len(path) > 1 else newmap
                out += ent(f, inner)
            else:
                out += d[p:s + n]          # unchanged entry, original header bytes
            p = s + n
        return bytes(out)
    # locate the path (field, length) down to the glyph map
    top = nbin.entries(d)
    f2 = [e for e in top if e[0] == 2][0]
    l1 = nbin.entries(d, f2[1], f2[1] + f2[2]); g = [e for e in l1 if e[0] == 1][0]
    l2 = nbin.entries(d, g[1], g[1] + g[2]); body = [e for e in l2 if e[0] == 1][0]
    fs = nbin.entries(d, body[1], body[1] + body[2]); m = [e for e in fs if e[0] == 1 and e[2] > 4][0]
    path = [(2, f2[2]), (1, g[2]), (1, body[2]), (1, m[2])]
    return sub(0, len(d), path)

def glyph_map(glyphs):
    """glyphs: list of (char, (x1,y1,x2,y2,a,b,c)) -> hash_map bytes"""
    n = len(glyphs)
    buckets = next(p for p in PRIMES if p >= n)   # SGI hash_map growth: smallest prime >= size
    b = ent(3, struct.pack('<i', n)) + ent(4, struct.pack('<i', buckets))
    # serialized in hash_map iteration order (bucket = code % buckets, last bucket first): HoF reads it back as is
    glyphs = sorted(glyphs, key=lambda g: (-(g[0] % buckets), g[0]))
    for ch, v in glyphs: b += ent(1, struct.pack('<H', ch))
    for ch, v in glyphs: b += ent(2, struct.pack('<7i', *v))
    return b

FMT = os.environ.get('H5JP_FMT', 'dxt3')

def dxt3(w, h, a):
    """alpha bytes of white text -> DXT3: 4-bit alpha per pixel, colour white..black by coverage (premultiplied look)"""
    import numpy as np
    A = np.frombuffer(a, np.uint8).reshape(h, w).astype(np.int32)
    B = A.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3).reshape(h // 4, w // 4, 16)
    a4 = (B * 15 + 127) // 255
    alpha = (a4[..., 0::2] | (a4[..., 1::2] << 4)).astype(np.uint8)
    lvl = (B * 3 + 127) // 255
    idx = np.choose(lvl, [1, 3, 2, 0])          # c0 = white, c1 = black; 0 = c0, 1 = c1, 2 = 2/3 c0, 3 = 1/3 c0
    bits = np.zeros(idx.shape[:2], np.uint32)
    for k in range(16): bits |= (idx[..., k].astype(np.uint32) << (2 * k))
    col = np.zeros(idx.shape[:2] + (4,), np.uint8); col[..., 0] = 0xff; col[..., 1] = 0xff
    blk = np.concatenate([alpha, col, bits.astype('<u4').view(np.uint8).reshape(idx.shape[:2] + (4,))], axis=2)
    return blk.tobytes()

def dds(w, h, a):
    """a = coverage bytes (L image) of white glyphs"""
    if FMT == 'dxt3':
        hdr = b'DDS ' + struct.pack('<7I', 124, 0x81007, h, w, w * h, 0, 1) + b'\0' * 44
        hdr += struct.pack('<8I', 32, 0x4, 0x33545844, 0, 0, 0, 0, 0)
        hdr += struct.pack('<5I', 0x1000, 0, 0, 0, 0)
        return hdr + dxt3(w, h, a)
    im = Image.frombytes('L', (w, h), a)
    hdr = b'DDS ' + struct.pack('<7I', 124, 0x100f | 0x8, h, w, w * 4, 0, 1) + b'\0' * 44
    hdr += struct.pack('<8I', 32, 0x41, 0, 32, 0xff0000, 0xff00, 0xff, 0xff000000)
    hdr += struct.pack('<5I', 0x1000, 0, 0, 0, 0)
    return hdr + Image.merge('RGBA', (im, im, im, im)).tobytes('raw', 'BGRA')

TEX_XDB = """<?xml version="1.0" encoding="UTF-8"?>
<Texture>
	<SrcName href="/Fonts/{name}.tga"/>
	<DestName href="{name}.(Texture).dds"/>
	<Type>REGULAR</Type>
	<ConversionType>CONVERT_TRANSPARENT</ConversionType>
	<AddrType>CLAMP</AddrType>
	<Format>TF_DXT3</Format>
	<Width>{w}</Width>
	<Height>{h}</Height>
	<MappingSize>0</MappingSize>
	<NMips>1</NMips>
	<Gain>0</Gain>
	<AverageColor>0</AverageColor>
	<InstantLoad>true</InstantLoad>
	<IsDXT>true</IsDXT>
	<FlipY>false</FlipY>
	<StandardExport>true</StandardExport>
	<UseS3TC>true</UseS3TC>
</Texture>
"""

FONT_FIELDS = ['uid', 'Height', 'Thickness', 'Italic', 'Antialiased', 'Pitch', 'Charset', 'FaceName', 'Name']

def font_xdb(x):
    """(Font).xdb written from its field values (same schema as the game's)"""
    tex = re.search(r'<Texture href="([^"]*)"', x).group(1)
    out = ['<?xml version="1.0" encoding="UTF-8"?>', '<Font>', '\t<Texture href="%s"/>' % tex]
    for f in FONT_FIELDS:
        m = re.search(r'<%s>(.*?)</%s>' % (f, f), x)
        out.append('\t<%s>%s</%s>' % (f, m.group(1), f) if m else '\t<%s/>' % f)
    out += ['\t<CharactersFile href=""/>', '</Font>', '']
    return '\n'.join(out)

def is_wide(ch): return ch >= 0x2e80 or 0xff00 <= ch <= 0xffef

def render_font(face, height, hdr, chars, weight):
    # the engine does not draw the top hdr[0x10] rows (internal leading) of a cell: keep every glyph below them
    H = hdr[0xa]; asc = hdr[0x11]; L = hdr[0x10]
    path = SANS if face in ('Century Gothic', 'Tahoma') else SERIF
    em = min(height, H - L)
    if face == 'Monotype Corsiva': em = min(em, int(round(height * 0.84)))   # the Latin script font is narrow
    ft = ImageFont.truetype(path, em)
    try: ft.set_variation_by_axes([weight])
    except Exception: pass
    jtop = L + (H - L - em) // 2                 # Japanese em box
    jbase = jtop + int(round(em * 0.88))
    lbase = max(asc, L + int(round(em * 0.80)))  # Latin baseline
    lbase = min(lbase, H - int(round(em * 0.22)))
    cells = []
    for ch in chars:
        c = chr(ch)
        if ch == 0x1f: c = '?'
        try: adv = ft.getlength(c)
        except Exception: adv = 0
        w = max(1, int(round(adv)))
        if is_wide(ch): w = em + 1 if em >= 14 else em
        if ch == 0x20: w = max(1, int(round(em * 0.08)))   # ASCII space = narrow break point (inserted between Japanese characters)
        if ch == 0xa0: c = ' '; w = max(2, int(round(em * 0.3)))   # NBSP = the visible space
        cells.append((ch, c, w))
    W = 1024 if H < 24 else 2048
    x = y = 0; pos = []
    for ch, c, w in cells:
        if x + w + 2 > W: x = 0; y += H + 2
        pos.append((x, y)); x += w + 2
    Ht = 64
    while Ht < y + H + 2: Ht *= 2
    im = Image.new('L', (W, Ht), 0)
    glyphs = []
    for (ch, c, w), (px, py) in zip(cells, pos):
        if c.strip() and w > 0 and ch != 0x20:
            cell = Image.new('L', (w, H), 0); dr = ImageDraw.Draw(cell)
            if is_wide(ch):
                bw = ft.getlength(c); dr.text(((w - bw) / 2.0, jbase), c, font=ft, fill=255, anchor='ls')
            else:
                dr.text((0, lbase), c, font=ft, fill=255, anchor='ls')
            if L: cell.paste(0, (0, 0, w, L))
            im.paste(cell, (px, py))
        glyphs.append((ch, (px, py, px + w, py + H, 0, w, w)))

    rgba = im.tobytes()
    return W, Ht, rgba, glyphs, em

class Paks:
    """several paks, newest file wins (as the game does)"""
    def __init__(self, paks):
        self.f = {}
        for p in ([paks] if isinstance(paks, str) else paks):
            z = zipfile.ZipFile(p)
            for i in z.infolist():
                k = i.filename.lower()
                if k not in self.f or self.f[k][1].date_time < i.date_time: self.f[k] = (z, i)
    def namelist(self): return [i.filename for z, i in self.f.values()]
    def read(self, n): z, i = self.f[n.lower()]; return z.read(i)

def main(pak, charset, out, only=None):
    z = Paks(pak)
    names = {n.lower(): n for n in z.namelist()}
    chars = sorted(set(range(0x20, 0x7f)) | {0x1f} | set(ord(c) for c in open(charset, encoding='utf-8').read() if ord(c) > 0x7e and ord(c) < 0x10000))
    files = {}
    atlases = {}
    for n in sorted(z.namelist()):
        if not n.endswith('(Font).xdb'): continue
        x = z.read(n).decode()
        name = n.split('/')[-1][:-len('.(Font).xdb')]
        if only and name not in only: continue
        uid = re.search('<uid>(.*?)</uid>', x).group(1)
        bn = names.get(('bin/fonts/' + uid).lower())
        if not bn: continue
        d = z.read(bn)
        hdr, orig = h5font.parse(d)
        fm = re.search('<FaceName>(.*?)<', x); height = int(re.search('<Height>(.*?)<', x).group(1))
        face = fm.group(1) if fm else ('Tahoma' if name.startswith('Button') else 'Century Gothic' if name.startswith('System') else 'Monotype Corsiva')
        keep = sorted(set(chars) | set(orig))
        W, Ht, rgba, glyphs, em = render_font(face, height, hdr, keep, 600 if face == 'Monotype Corsiva' else 500)
        files[bn] = rebuild(d, glyph_map(glyphs))
        dd = dds(W, Ht, rgba)
        same = atlases.get(dd)
        if same:
            # identical atlas (e.g. Header_40 = Default_40): point the font at that texture instead of a second copy
            x = re.sub(r'<Texture href="[^"]*"', '<Texture href="%s.(Texture).xdb#xpointer(/Texture)"' % same, x)
        # Hammers of Fate takes bin/fonts/<uid> from the pak that holds the winning (Font).xdb: ship our own copy
        files[n] = font_xdb(x).encode()
        if same:
            print(name, 'shares the atlas of', same); continue
        atlases[dd] = name
        tx = TEX_XDB.format(name=name, w=W, h=Ht)
        if FMT != 'dxt3': tx = tx.replace('TF_DXT3', 'TF_8888').replace('<IsDXT>true', '<IsDXT>false')
        files[n.replace('(Font).xdb', '(Texture).xdb')] = tx.encode()
        files[n.replace('(Font).xdb', '(Texture).dds')] = dd
        print(name, face, height, 'em', em, 'atlas', W, Ht, len(glyphs), 'glyphs')
    if out is None: return files
    os.makedirs(out, exist_ok=True)
    for k, v in files.items():
        p = os.path.join(out, k.replace('/', os.sep)); os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(v)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:] or None)
