# Noto Serif JP (OFL) -> static instance, subset to the characters in build/tr.bin (plus kana/symbols)
# -> build/h4jp_font.ttf (resource H4JP/2, family "H4JP Serif")
import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SRC = r'C:\Windows\Fonts\NotoSerifJP-VF.ttf'
WEIGHT = 600

def euc_chars(blob):
    out = set(); i = 0
    while i < len(blob) - 1:
        a, b = blob[i], blob[i + 1]
        if 0xa1 <= a <= 0xfe and 0xa1 <= b <= 0xfe:
            try: out.add(bytes([a, b]).decode('euc_jp'))
            except UnicodeDecodeError: pass
            i += 2
        else: i += 1
    return out

def main():
    p = os.path.join(ROOT, 'build', 'tr.bin')
    blob = open(p, 'rb').read() if os.path.exists(p) else b''
    chars = euc_chars(blob)
    extra = bytearray()
    for a in range(0xa1, 0xa9):            # JIS rows 1-8: symbols, kana, greek, cyrillic, box
        for b in range(0xa1, 0xff): extra += bytes([a, b])
    if len(sys.argv) > 1 and sys.argv[1] == 'level1':
        for a in range(0xb0, 0xd0):
            for b in range(0xa1, 0xff): extra += bytes([a, b])
    chars |= euc_chars(bytes(extra))
    print('chars', len(chars))
    f = TTFont(SRC)
    f = instancer.instantiateVariableFont(f, {'wght': WEIGHT})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = []; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=[ord(c) for c in chars])
    s.subset(f)
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16): rec.string = 'H4JP Serif'
        elif rec.nameID == 6: rec.string = 'H4JPSerif'
    out = os.path.join(ROOT, 'build', 'h4jp_font.ttf')
    f.save(out)
    print(os.path.getsize(out), 'bytes')

if __name__ == '__main__':
    main()
