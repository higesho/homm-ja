# Build a Scaleform (GFx) font library SWF with DefineFont3 glyphs from Noto Sans JP (OFL).
#   make_fontlib(chars) -> bytes of a "GFX" movie holding font "H7JP Sans" (regular) and "H7JP Sans" Bold
import struct, math
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools.pens.basePen import BasePen

NOTO = r'C:\Windows\Fonts\NotoSansJP-VF.ttf'
FONTNAME = 'H7JP Sans'
EM = 1024 * 20          # DefineFont3 glyph space
LATIN_X = 0.88          # Latin letters, digits and symbols are drawn narrower: the game's fields were sized for Calibri/Fontin

class Bits:
    def __init__(self): self.buf = bytearray(); self.acc = 0; self.n = 0
    def put(self, v, nb):
        for i in range(nb - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1); self.n += 1
            if self.n == 8: self.buf.append(self.acc); self.acc = 0; self.n = 0
    def sput(self, v, nb): self.put(v & ((1 << nb) - 1), nb)
    def flush(self):
        if self.n: self.buf.append(self.acc << (8 - self.n)); self.acc = 0; self.n = 0
        return bytes(self.buf)

def sbits(*vals):
    m = max(abs(v) for v in vals) if vals else 0
    n = 1
    while not (-(1 << (n - 1)) <= min(vals) and max(vals) <= (1 << (n - 1)) - 1): n += 1
    return max(n, 2)

class ShapePen(BasePen):
    """records contours as lists of ('L', x, y) / ('Q', cx, cy, x, y) in SWF units (y down)"""
    def __init__(self, gs, scale, sx=None):
        super().__init__(gs); self.s = scale; self.sx = sx or scale; self.contours = []; self.cur = None
    def _p(self, pt): return (int(round(pt[0] * self.sx)), int(round(-pt[1] * self.s)))
    def _moveTo(self, pt): self.cur = [('M',) + self._p(pt)]; self.contours.append(self.cur)
    def _lineTo(self, pt): self.cur.append(('L',) + self._p(pt))
    def _qCurveToOne(self, p1, p2): self.cur.append(('Q',) + self._p(p1) + self._p(p2))
    def _curveToOne(self, p1, p2, p3):
        # cubic -> two quadratics (Noto Sans JP VF is quadratic; kept for safety)
        x0, y0 = self._getCurrentPoint()
        mid = ((x0 + 3 * p1[0] + 3 * p2[0] + p3[0]) / 8, (y0 + 3 * p1[1] + 3 * p2[1] + p3[1]) / 8)
        c1 = ((x0 + p1[0]) / 2 * 0.5 + (p1[0] + p2[0]) / 2 * 0.5, (y0 + p1[1]) / 2 * 0.5 + (p1[1] + p2[1]) / 2 * 0.5)
        c2 = ((p1[0] + p2[0]) / 2 * 0.5 + (p2[0] + p3[0]) / 2 * 0.5, (p1[1] + p2[1]) / 2 * 0.5 + (p2[1] + p3[1]) / 2 * 0.5)
        self.cur.append(('Q',) + self._p(c1) + self._p(mid)); self.cur.append(('Q',) + self._p(c2) + self._p(p3))
    def _closePath(self):
        if self.cur and len(self.cur) > 1:
            x, y = self.cur[0][1], self.cur[0][2]
            last = self.cur[-1]
            lx, ly = (last[1], last[2]) if last[0] in 'ML' else (last[3], last[4])
            if (lx, ly) != (x, y): self.cur.append(('L', x, y))
    _endPath = _closePath

def shape_bytes(contours):
    b = Bits()
    b.put(1, 4); b.put(0, 4)                 # NumFillBits=1, NumLineBits=0
    x = y = 0; first = True
    for c in contours:
        if len(c) < 2: continue
        mx, my = c[0][1], c[0][2]
        nb = sbits(mx, my) if (mx or my) else 1
        # StyleChangeRecord: TypeFlag 0, NewStyles 0, LineStyle 0, FillStyle1 0, FillStyle0 first, MoveTo 1
        b.put(0, 1); b.put(0, 1); b.put(0, 1); b.put(0, 1); b.put(1 if first else 0, 1); b.put(1, 1)
        b.put(nb, 5); b.sput(mx, nb); b.sput(my, nb)
        if first: b.put(1, 1); first = False      # FillStyle0 = 1
        x, y = mx, my
        for e in c[1:]:
            if e[0] == 'L':
                dx, dy = e[1] - x, e[2] - y
                if dx == 0 and dy == 0: continue
                if dx and dy:
                    nb = sbits(dx, dy)
                    b.put(1, 1); b.put(1, 1); b.put(nb - 2, 4); b.put(1, 1); b.sput(dx, nb); b.sput(dy, nb)
                else:
                    nb = sbits(dx or dy)
                    b.put(1, 1); b.put(1, 1); b.put(nb - 2, 4); b.put(0, 1); b.put(1 if dy else 0, 1); b.sput(dy if dy else dx, nb)
                x, y = e[1], e[2]
            else:
                cx, cy, ax, ay = e[1] - x, e[2] - y, e[3] - e[1], e[4] - e[2]
                nb = sbits(cx, cy, ax, ay)
                b.put(1, 1); b.put(0, 1); b.put(nb - 2, 4); b.sput(cx, nb); b.sput(cy, nb); b.sput(ax, nb); b.sput(ay, nb)
                x, y = e[3], e[4]
    b.put(0, 6)                               # EndShapeRecord
    return b.flush()

def rect_bytes(x0, x1, y0, y1):
    nb = sbits(x0, x1, y0, y1)
    b = Bits(); b.put(nb, 5)
    for v in (x0, x1, y0, y1): b.sput(v, nb)
    return b.flush()

def tag(code, data):
    if len(data) < 0x3f and code not in (75,):
        return struct.pack('<H', (code << 6) | len(data)) + data
    return struct.pack('<HI', (code << 6) | 0x3f, len(data)) + data

def instance(wght):
    f = TTFont(NOTO)
    return instancer.instantiateVariableFont(f, {'wght': wght})

def define_font3(fid, font, chars, bold):
    cmap = font.getBestCmap(); gs = font.getGlyphSet(); hmtx = font['hmtx']
    upm = font['head'].unitsPerEm; s = EM / upm
    codes = sorted(c for c in set(chars) if c in cmap)
    shapes = []; adv = []; bounds = []
    for c in codes:
        gn = cmap[c]
        sx = s * (LATIN_X if c < 0x2E80 else 1.0)
        pen = ShapePen(gs, s, sx); gs[gn].draw(pen)
        shapes.append(shape_bytes(pen.contours))
        adv.append(min(32767, int(round(hmtx[gn][0] * sx))))
        pts = [(e[-2], e[-1]) for cc in pen.contours for e in cc] + [(e[1], e[2]) for cc in pen.contours for e in cc if e[0] == 'Q']
        if pts:
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            bounds.append(rect_bytes(min(xs), max(xs), min(ys), max(ys)))
        else:
            bounds.append(rect_bytes(0, 0, 0, 0))
    n = len(codes)
    name = FONTNAME.encode('utf-8')
    off = 4 * n + 4
    offsets = []
    for sh in shapes: offsets.append(off); off += len(sh)
    codetab = off
    flags = 0x80 | 0x08 | 0x04 | (0x01 if bold else 0)       # HasLayout, WideOffsets, WideCodes, Bold
    d = bytearray(struct.pack('<HBB', fid, flags, 2) + bytes([len(name)]) + name + struct.pack('<H', n))
    d += struct.pack('<%dI' % n, *offsets) + struct.pack('<I', codetab)
    for sh in shapes: d += sh
    d += struct.pack('<%dH' % n, *codes)
    asc = int(round(880 * s)); desc = int(round(240 * s))
    d += struct.pack('<HHh', asc, desc, 0)
    d += struct.pack('<%dh' % n, *adv)
    for r in bounds: d += r
    d += struct.pack('<H', 0)                 # no kerning
    return tag(75, bytes(d)), n

def make_fontlib(chars, name='h7jpfont'):
    chars = set(ord(c) if isinstance(c, str) else c for c in chars)
    chars |= set(range(0x20, 0x7f)) | set(range(0xa0, 0x100))
    body = bytearray()
    nm = name.encode()
    body += tag(1000, struct.pack('<HIH', 0x0401, 0, 0x0d) + b'\x00' + bytes([len(nm)]) + nm + b'\x00\x00')
    body += tag(69, struct.pack('<I', 0x08))
    body += tag(9, b'\xff\xff\xff')
    body += tag(86, b'\x01\x00Scene 1\x00\x00')
    total = 0
    for fid, (w, bold) in enumerate([(500, False), (700, True)], 1):
        f = instance(w)
        t, n = define_font3(fid, f, chars, bold)
        body += t; total = n
    body += tag(1, b'') + tag(0, b'')
    hdr_rect = rect_bytes(0, 11000, 0, 8000)
    head = b'GFX' + bytes([10])
    rest = hdr_rect + struct.pack('<HH', 0x0c00, 1) + bytes(body)
    return head + struct.pack('<I', 8 + len(rest)) + rest, total
