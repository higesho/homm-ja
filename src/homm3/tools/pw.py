# capture the window titled "H3JP TEST" with PrintWindow (works even when covered)
import ctypes, sys, os
from ctypes import wintypes
from PIL import Image
u=ctypes.windll.user32; g=ctypes.windll.gdi32
def grab(title='H3JP TEST', out=None):
    h=u.FindWindowA(None,title.encode())
    if not h: raise SystemExit('window not found')
    r=wintypes.RECT(); u.GetClientRect(h,ctypes.byref(r)); w,hh=r.right,r.bottom
    dc=u.GetDC(h); mdc=g.CreateCompatibleDC(dc); bm=g.CreateCompatibleBitmap(dc,w,hh); g.SelectObject(mdc,bm)
    ok=u.PrintWindow(h,mdc,3)
    class BIH(ctypes.Structure): _fields_=[('biSize',ctypes.c_uint32),('biWidth',ctypes.c_int32),('biHeight',ctypes.c_int32),('biPlanes',ctypes.c_uint16),('biBitCount',ctypes.c_uint16),('biCompression',ctypes.c_uint32),('biSizeImage',ctypes.c_uint32),('a',ctypes.c_int32),('b',ctypes.c_int32),('c',ctypes.c_uint32),('d',ctypes.c_uint32)]
    bi=BIH(); bi.biSize=40; bi.biWidth=w; bi.biHeight=-hh; bi.biPlanes=1; bi.biBitCount=32
    buf=ctypes.create_string_buffer(w*hh*4); g.GetDIBits(mdc,bm,0,hh,buf,ctypes.byref(bi),0)
    im=Image.frombuffer('RGBA',(w,hh),buf,'raw','BGRA',0,1).convert('RGB')
    g.DeleteObject(bm); g.DeleteDC(mdc); u.ReleaseDC(h,dc)
    if out: im.save(out)
    return im, ok
if __name__=='__main__':
    im,ok=grab(out=sys.argv[1]); print(im.size, ok)
