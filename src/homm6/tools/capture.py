# Capture the Heroes VI window without bringing it to the front (PrintWindow, full content) -> PNG
#   python capture.py out.png [title]
import sys, ctypes, ctypes.wintypes as W, struct, zlib
u = ctypes.windll.user32; g = ctypes.windll.gdi32
u.SetProcessDPIAware()
title = sys.argv[2] if len(sys.argv) > 2 else 'Might & Magic Heroes VI'
h = u.FindWindowW(None, title)
if not h: sys.exit('window not found')
r = W.RECT(); u.GetClientRect(h, ctypes.byref(r))
w, hh = r.right, r.bottom
dc = u.GetDC(h); mdc = g.CreateCompatibleDC(dc); bmp = g.CreateCompatibleBitmap(dc, w, hh); g.SelectObject(mdc, bmp)
ok = u.PrintWindow(h, mdc, 3)  # PW_CLIENTONLY | PW_RENDERFULLCONTENT
class BMI(ctypes.Structure):
    _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD), ('biBitCount', W.WORD),
                ('biCompression', W.DWORD), ('biSizeImage', W.DWORD), ('a', W.LONG), ('b', W.LONG), ('c', W.DWORD), ('d', W.DWORD)]
bi = BMI(ctypes.sizeof(BMI), w, -hh, 1, 32, 0, 0, 0, 0, 0, 0)
buf = ctypes.create_string_buffer(w * hh * 4)
g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(h, dc)
raw = bytearray()
b = buf.raw
for y in range(hh):
    raw.append(0); row = b[y * w * 4:(y + 1) * w * 4]
    px = bytearray(w * 3); px[0::3] = row[2::4]; px[1::3] = row[1::4]; px[2::3] = row[0::4]
    raw += px
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, hh, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw), 6)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
print(w, hh, 'ok' if ok else 'PrintWindow failed')
