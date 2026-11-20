# Noto Serif JP (OFL) -> static instance subset to the given characters (+ ASCII, Latin-1, kana, JIS symbols)
#   make_font(chars) -> TTF bytes, family "H6JP Serif"
import io, os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
SRC = r'C:\Windows\Fonts\NotoSerifJP-VF.ttf'
FAMILY = 'H6JP Serif'

def base_chars(level1=False):
    s = set(chr(c) for c in range(0x20, 0x7f)) | set(chr(c) for c in range(0xa0, 0x180))
    s |= set('‐–—―‘’“”…•′″※←→↑↓')
    extra = bytearray()
    rows = list(range(0xa1, 0xa9)) + (list(range(0xb0, 0xd0)) if level1 else [])
    for a in rows:
        for b in range(0xa1, 0xff): extra += bytes([a, b])
    for i in range(0, len(extra), 2):
        try: s.add(extra[i:i + 2].decode('euc_jp'))
        except UnicodeDecodeError: pass
    return s

def make_font(chars, weight=600):
    f = TTFont(SRC)
    f = instancer.instantiateVariableFont(f, {'wght': weight})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = ['kern', 'palt'] and []; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=sorted(ord(c) for c in chars))
    s.subset(f)
    # Scaleform puts the baseline at the hhea ascent: Noto's 1.151/0.286 em pushed the text down and cut the descenders
    # of the fixed-height Fontin fields (0.8/0.2 em). Use the CJK top 0.88; descent 0.23 so wrapped lines do not touch.
    asc, desc = int(os.environ.get('H6JP_ASC', '880')), int(os.environ.get('H6JP_DESC', '230'))
    h = f['hhea']; h.ascent, h.descent, h.lineGap = asc, -desc, 0
    o = f['OS/2']; o.sTypoAscender, o.sTypoDescender, o.sTypoLineGap = asc, -desc, 0; o.usWinAscent, o.usWinDescent = asc, desc
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16): rec.string = FAMILY
        elif rec.nameID == 6: rec.string = FAMILY.replace(' ', '')
    b = io.BytesIO(); f.save(b)
    return b.getvalue()
