# Build our font packages (all glyphs drawn from Noto Sans JP, OFL):
#   work/stage/CookedPC/H7JP/H7JPFont.upk : GFx font library (SwfMovie H7JPFont.lib.fonts_jp, fonts "H7JP Sans" regular+bold)
#   work/stage/CookedPC/H7JP/EngineFonts_JP.upk : replacement for EngineFonts (SmallFont/TinyFont bitmap fonts with Japanese),
#       the DLL redirects EngineFonts.upk here (subtitles of the cut-scene movies use EngineFonts.SmallFont)
#   python tools/make_fonts.py [gfx|bitmap|all]
import os, sys, struct, re, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from upkwrite import Package, fstr
import swffont
from intfile import ROOT, read_text

OUT = os.path.join(ROOT, 'work', 'stage', 'CookedPC', 'H7JP')
ENGINEFONTS_GUID = bytes.fromhex('3fc62c0121976b4fae8f085eb00eb854')   # GUID of the original EngineFonts.upk

def used_chars():
    s = set()
    for p in glob.glob(os.path.join(ROOT, 'work', 'stage', '*', 'JPN', '*.JPN')):
        s |= set(read_text(p))
    s |= set(chr(c) for c in range(0x20, 0x7f)) | set(chr(c) for c in range(0xa0, 0x100))
    # strings we leave English (credits) are read from the INT files: their letters must exist too
    from intfile import int_files
    for area, fn, path in int_files():
        s |= set(read_text(path))
    # mod texts (UCP, Heroes 7.5): our JPN files for them and their own INT files (tools/modloc.py)
    for p in glob.glob(os.path.join(ROOT, 'work', 'stage_*', 'Localization', '*', 'JPN', '*.JPN')) + \
             glob.glob(os.path.join(ROOT, 'work', '*', 'x', 'Localization', '*', 'INT', '*.int')):
        s |= set(read_text(p))
    s |= set('、。「」『』（）！？：・…ー―　〜々')
    return {c for c in s if ord(c) >= 0x20}

def build_gfx(chars):
    swf, n = swffont.make_fontlib(chars)
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(ROOT, 'work', 'h7jpfont.gfx'), 'wb').write(swf)
    P = Package()
    pkgcls = P.class_ref('Core', 'Package')
    swfcls = P.class_ref('GFxUI', 'SwfMovie')
    lib = P.exp(pkgcls, 'lib', lambda p, pos, k: struct.pack('<i', k) + p.none(), oflags=0x0007000400000000)
    def movie(p, pos, k):
        d = struct.pack('<i', k)
        d += p.p_str('SourceFile', '..\\..\\MMH7Game\\Flash\\H7JP\\lib\\fonts_jp.swf')
        d += p.p_array('RawData', len(swf), swf)
        d += p.none()
        return d
    P.exp(swfcls, 'fonts_jp', movie, outer=lib)
    sz = P.save(os.path.join(OUT, 'H7JPFont.upk'))
    print('H7JPFont.upk', sz, 'bytes,', n, 'glyphs per font')

# ---------------- bitmap fonts (UE3 Font + DXT5 Texture2D pages) ----------------
def dxt5(alpha):
    """alpha: uint8 array HxW (multiples of 4) -> DXT5 bytes, colour white"""
    import numpy as np
    h, w = alpha.shape
    b = alpha.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3).reshape(-1, 16).astype(np.int32)
    a0 = b.max(1); a1 = b.min(1)
    k = np.arange(8)
    # palette for a0 > a1: [a0, a1, (6a0+a1)/7, (5a0+2a1)/7, ..., (a0+6a1)/7]
    pal = np.zeros((len(b), 8), np.int32)
    pal[:, 0] = a0; pal[:, 1] = a1
    for i in range(1, 7):
        pal[:, i + 1] = ((7 - i) * a0 + i * a1) // 7
    idx = np.abs(b[:, :, None] - pal[:, None, :]).argmin(2)          # (n,16)
    idx[a0 == a1] = 0
    bits = np.zeros(len(b), np.uint64)
    for i in range(16):
        bits |= idx[:, i].astype(np.uint64) << np.uint64(3 * i)
    out = np.zeros((len(b), 16), np.uint8)
    out[:, 0] = a0; out[:, 1] = a1
    for i in range(6):
        out[:, 2 + i] = ((bits >> np.uint64(8 * i)) & np.uint64(0xff)).astype(np.uint8)
    out[:, 8:12] = 0xff                                               # colour endpoints: white, white
    return out.tobytes()

def render_pages(chars, px, wght, page=1024, pad=1):
    """-> (chars_info list in order of codes, pages (alpha arrays), cell height, codes)"""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    f = ImageFont.truetype(swffont.NOTO, px)
    try: f.set_variation_by_axes([wght])
    except Exception: pass
    asc, desc = f.getmetrics()
    H = asc + desc
    codes = sorted(set(ord(c) for c in chars) | {0x20, 0x3f})
    pages = []; info = []
    img = None; x = y = 0
    def newpage():
        im = Image.new('L', (page, page), 0)
        pages.append(im); return im, ImageDraw.Draw(im)
    img, dr = newpage()
    for c in codes:
        ch = chr(c)
        adv = int(round(f.getlength(ch))) or (px // 3)
        w = adv + 2 * pad
        if x + w > page:
            x = 0; y += H + 2 * pad
        if y + H + 2 * pad > page:
            img, dr = newpage(); x = y = 0
        dr.text((x + pad, y + pad), ch, font=f, fill=255)
        info.append((c, x + pad, y + pad, adv, H, len(pages) - 1))
        x += w
    return info, [np.array(p) for p in pages], H

def font_object(P, info, tex_exports):
    def build(p, pos, k):
        d = struct.pack('<i', k)
        chars = b''.join(struct.pack('<iiiiBi', u, v, w, h, t, 0) for (c, u, v, w, h, t) in info)
        d += p.p_array('Characters', len(info), chars)
        d += p.p_array('Textures', len(tex_exports), b''.join(struct.pack('<i', t) for t in tex_exports))
        d += p.p_int('IsRemapped', 1)                      # an int (UBOOL) property in this engine build
        d += p.none()
        d += struct.pack('<i', len(info)) + b''.join(struct.pack('<HH', c, i) for i, (c, *_) in enumerate(info))
        return d
    return build

def texture_object(alpha):
    import uuid
    h, w = alpha.shape
    data = dxt5(alpha)
    g1, g2 = uuid.uuid4().bytes, uuid.uuid4().bytes
    def build(p, pos, k):
        d = struct.pack('<i', k)
        d += p.p_int('SizeX', w) + p.p_int('SizeY', h) + p.p_int('OriginalSizeX', w) + p.p_int('OriginalSizeY', h)
        d += p.p_byte_enum('Format', 'EPixelFormat', 'PF_DXT5')
        d += p.p_bool('NeverStream', True)
        d += p.p_byte_enum('LODGroup', 'TextureGroup', 'TEXTUREGROUP_UI')
        d += p.p_byte_enum('MipGenSettings', 'TextureMipGenSettings', 'TMGS_NoMipmaps')
        d += p.p_struct('LightingGuid', 'Guid', g1)
        d += p.none()
        d += struct.pack('<Iiii', 0, 0, 0, pos + len(d) + 16)                      # SourceArt (empty)
        d += struct.pack('<i', 1)                                                    # one mip
        d += struct.pack('<Iiii', 0, len(data), len(data), pos + len(d) + 16) + data
        d += struct.pack('<ii', w, h)
        d += g2                                                                      # TextureFileCacheGuid
        d += struct.pack('<iii', 0, max(w, h), 0)                                    # PVRTC mips, flash max res, ATITC mips
        d += struct.pack('<Iiii', 0x21, 0, -1, -1)                                   # cached flash mips (unused)
        d += struct.pack('<i', 0)                                                    # ETC mips
        return d
    return build

def build_bitmap(chars, small_px=22, tiny_px=13):
    os.makedirs(OUT, exist_ok=True)
    P = Package(guid=ENGINEFONTS_GUID)
    fontcls = P.class_ref('Engine', 'Font')
    texcls = P.class_ref('Engine', 'Texture2D')
    sinfo, spages, sh = render_pages(chars, small_px, 500)
    tinfo, tpages, th = render_pages([chr(c) for c in range(0x20, 0x7f)] + [chr(c) for c in range(0xa0, 0x100)], tiny_px, 500, page=256)
    # fonts must be exports 1 and 2 (like the original); textures follow
    ns, nt = len(spages), len(tpages)
    stex = [3 + i for i in range(ns)]; ttex = [3 + ns + i for i in range(nt)]
    P.exp(fontcls, 'SmallFont', font_object(P, sinfo, stex))
    P.exp(fontcls, 'TinyFont', font_object(P, tinfo, ttex))
    for i, a in enumerate(spages): P.exp(texcls, 'SmallFont_Page%d' % i, texture_object(a), outer=1, oflags=0x0007000400000000)
    for i, a in enumerate(tpages): P.exp(texcls, 'TinyFont_Page%d' % i, texture_object(a), outer=2, oflags=0x0007000400000000)
    sz = P.save(os.path.join(OUT, 'EngineFonts_JP.upk'))
    print('EngineFonts_JP.upk', sz, 'bytes;', len(sinfo), 'glyphs on', ns, 'pages, cell height', sh)

if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'all'
    ch = used_chars()
    print(len(ch), 'characters')
    if what in ('gfx', 'all'): build_gfx(ch)
    if what in ('bitmap', 'all'): build_bitmap(ch)
