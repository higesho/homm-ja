import struct, sys, glob, os
def load(p):
    d=open(p,'rb').read()
    h=d[:0x20]
    w=[struct.unpack_from('<3i',d,0x20+12*i) for i in range(256)]
    offs=struct.unpack_from('<256I',d,0xC20)
    return d,h,w,offs
if __name__=='__main__':
    for p in sorted(glob.glob('ext/H3bitmap/*.fnt')+glob.glob('ext/H3bitmap/*.FNT')):
        d,h,w,offs=load(p)
        print(os.path.basename(p), len(d), h[:8].hex(), 'height',h[5], 'A',w[65],'a',w[97],'sp',w[32])
    d,h,w,offs=load(sys.argv[1]) if len(sys.argv)>1 else load('ext/H3bitmap/smalfont.fnt')
    H=h[5]
    for ch in 'Ag':
        c=ord(ch); l,wd,r=w[c]; o=0x1020+offs[c]
        for y in range(H): print(''.join('.#s'[0 if v==0 else 1 if v==255 else 2] for v in d[o+y*wd:o+(y+1)*wd]))
        print()
