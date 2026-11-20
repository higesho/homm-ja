# HoMM4 font .h4d: u8 ver, u8 ?, u8 first, u8 height, u8 b4, u8 b5, u8 count;
#   count * {i32 w, i32 h, i32 bearing, i32 extra advance, w*h bytes (low nibble: alpha of colour 1, high: colour 2)}
import sys, struct, collections
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import h4r
def parse(b):
    ver, x, first, hgt, b4, b5, cnt = b[:7]
    p = 7; g = []
    for i in range(cnt):
        w, h, be, ad = struct.unpack_from('<4i', b, p); p += 16
        g.append((w, h, be, ad, b[p:p + w * h])); p += w * h
    return dict(ver=ver, x=x, first=first, height=hgt, b4=b4, b5=b5, glyphs=g, rest=len(b) - p)
def fonts(path='../data/heroes4.h4r'):
    d, E = h4r.entries(path)
    return {e['name']: parse(h4r.read(d, e)) for e in E if e['name'].startswith('font.')}
if __name__ == '__main__':
    F = fonts()
    for n, f in F.items():
        hist = collections.Counter()
        for g in f['glyphs']: hist.update(g[4])
        print(n, f['first'], f['height'], f['b4'], f['b5'], len(f['glyphs']), 'rest', f['rest'], sorted(hist.items())[:20])
    f = F[sys.argv[1]] if len(sys.argv) > 1 else F['font.Prose_Antique.14.h4d']
    for ch in 'Ag':
        w, h, be, ad, px = f['glyphs'][ord(ch) - f['first']]
        print(ch, w, h, be, ad)
        for y in range(h): print(''.join('%02x' % px[y * w + x] if px[y * w + x] else ' .' for x in range(w)))
