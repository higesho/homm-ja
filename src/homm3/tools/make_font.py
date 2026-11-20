# build/glyphs.bin: JP glyph sets (all JIS X 0208) for the game's fonts
#  set0 misaki 8x8 (tiny), set1 shinonome 12 (small/times/calli/verd), set2 shinonome 14 (med),
#  set3 BIZ UDGothic Bold 18px (big/hiscore), set4 BIZ UDGothic Bold 24px (credits)
import struct, os
from PIL import Image, ImageDraw, ImageFont
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def bdf(path, enc):
    g={}; cur=None; rows=None; bbx=None
    for line in open(path, encoding='latin-1'):
        t=line.split()
        if not t: continue
        if t[0]=='ENCODING': cur=int(t[1])
        elif t[0]=='BBX': bbx=list(map(int,t[1:5]))
        elif t[0]=='BITMAP': rows=[]
        elif t[0]=='ENDCHAR':
            if cur is not None and rows is not None: g[cur]=(bbx,rows)
            rows=None; cur=None
        elif rows is not None: rows.append((int(t[0],16), len(t[0])*4))
    return g
def jis_to_euc(j): return j|0x8080
def all_euc():
    for a in range(0xA1,0xFF):
        for b in range(0xA1,0xFF):
            try:
                c=bytes([a,b]).decode('euc_jp')
                yield (a<<8)|b, c
            except Exception: pass
def from_bdf(path, size, unicode_enc, fbb_desc=-2):
    g=bdf(path, None); out={}
    for code, ch in all_euc():
        key = ord(ch) if unicode_enc else (code & 0x7f7f)
        if key not in g: continue
        (bw,bh,bx,by),rows=g[key]
        img=[[0]*size for _ in range(size)]
        # place by bounding box: baseline at size+fbb_desc
        top = size + fbb_desc - (by + bh)
        for r,(v,bits) in enumerate(rows):
            for x in range(bw):
                if v>>(bits-1-x)&1:
                    yy=top+r; xx=bx+x
                    if 0<=yy<size and 0<=xx<size: img[yy][xx]=1
        out[code]=img
    return out
def from_ttf(path, idx, size, px):
    f=ImageFont.truetype(path, px, index=idx); out={}
    base=round(size*0.86)
    for code, ch in all_euc():
        im=Image.new('1',(size,size),0); d=ImageDraw.Draw(im); d.fontmode='1'
        d.text((size/2, base), ch, font=f, fill=1, anchor='ms')
        px_=im.load()
        img=[[1 if px_[x,y] else 0 for x in range(size)] for y in range(size)]
        if any(any(r) for r in img) or ch in '\u3000': out[code]=img
    return out
def pack(sets):
    hdr=b'H3GL'+struct.pack('<I',len(sets)); body=b''; base=8+16*len(sets); ents=[]
    for (w,h,adv,gl) in sets:
        codes=sorted(gl); bpr=(w+7)//8
        cb=b''.join(struct.pack('<H',c) for c in codes)
        if len(cb)%4: cb+=b'\0'*(4-len(cb)%4)
        bits=bytearray()
        for c in codes:
            for row in gl[c]:
                v=0
                for x in range(w): v|=row[x]<<(bpr*8-1-x)
                bits+=v.to_bytes(bpr,'big')
        off_codes=base+len(body); body+=cb; off_bits=base+len(body); body+=bytes(bits)
        while len(body)%4: body+=b'\0'
        ents.append(struct.pack('<BBBBIII',w,h,adv,0,len(codes),off_codes,off_bits))
    return hdr+b''.join(ents)+body
if __name__=='__main__':
    F=os.path.join(ROOT,'font')
    biz=r'C:\Windows\Fonts\BIZ-UDGothicB.ttc'
    sets=[(8,8,8,from_bdf(os.path.join(F,'misaki_gothic.bdf'),8,True)),
          (12,12,12,from_bdf(os.path.join(F,'shnmk12.bdf'),12,False)),
          (14,14,14,from_bdf(os.path.join(F,'shnmk14.bdf'),14,False)),
          (18,18,18,from_ttf(biz,0,18,17)),
          (24,24,24,from_ttf(biz,0,24,22))]
    for s in sets: print(s[0], len(s[3]))
    os.makedirs(os.path.join(ROOT,'build'),exist_ok=True)
    open(os.path.join(ROOT,'build','glyphs.bin'),'wb').write(pack(sets))
    # preview
    txt='英雄の城塞「グリフィン」、ゴールド。'
    im=Image.new('L',(30*24,5*26),0)
    for si,(w,h,adv,gl) in enumerate(sets):
        for i,ch in enumerate(txt):
            c=int.from_bytes(ch.encode('euc_jp'),'big'); g=gl.get(c)
            if not g: continue
            for y in range(h):
                for x in range(w):
                    if g[y][x]: im.putpixel((i*(adv+1)+x, si*26+y),255)
    im.save(os.path.join(ROOT,'work','glyphs_preview.png'))
