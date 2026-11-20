# Drive the HoMM4 test window (title "H4JP TEST") by PID. Game coords = client pixels.
#   python tools/win.py start [jp|none] | shot NAME | key K.. | click X Y [right] | stop
import sys, os, time, ctypes, ctypes.wintypes as W, subprocess, shutil
from ctypes import windll
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
PIDF = os.path.join(ROOT, 'test', 'pid.txt')
u32 = windll.user32; g32 = windll.gdi32
WNDENUMPROC = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def find_window(pid=None):
    if pid is None: pid = int(open(PIDF).read())
    found = []
    def cb(h, l):
        p = W.DWORD(); u32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u32.IsWindowVisible(h):
            n = u32.GetWindowTextLengthW(h); b = ctypes.create_unicode_buffer(n + 1); u32.GetWindowTextW(h, b, n + 1)
            found.append((h, b.value))
        return True
    u32.EnumWindows(WNDENUMPROC(cb), 0)
    if not found: raise SystemExit('window of pid %d not found' % pid)
    for f in found:
        if f[1] == 'H4JP TEST': return f
    return found[0]
def client_size(h):
    r = W.RECT(); u32.GetClientRect(h, ctypes.byref(r)); return r.right, r.bottom
def hide():
    try:
        h, t = find_window(); r = W.RECT(); u32.GetWindowRect(h, ctypes.byref(r))
        if r.left < 2600: u32.SetWindowPos(h, 1, 2700, 100, 0, 0, 0x15)
    except SystemExit: pass
def grab():
    from PIL import Image
    h, t = find_window(); w, hh = client_size(h)
    hdc = u32.GetDC(h); mdc = g32.CreateCompatibleDC(hdc); bmp = g32.CreateCompatibleBitmap(hdc, w, hh); g32.SelectObject(mdc, bmp)
    if not u32.PrintWindow(h, mdc, 3): g32.BitBlt(mdc, 0, 0, w, hh, hdc, 0, 0, 0x00CC0020)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD), ('biBitCount', W.WORD),
                    ('biCompression', W.DWORD), ('biSizeImage', W.DWORD), ('a', W.LONG), ('b', W.LONG), ('c', W.DWORD), ('d', W.DWORD)]
    bi = BIH(ctypes.sizeof(BIH), w, -hh, 1, 32, 0, 0, 0, 0, 0, 0); buf = ctypes.create_string_buffer(w * hh * 4)
    g32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0); g32.DeleteObject(bmp); g32.DeleteDC(mdc); u32.ReleaseDC(h, hdc)
    return Image.frombuffer('RGBA', (w, hh), buf.raw, 'raw', 'BGRA', 0, 1).convert('RGB'), t
def shot(name):
    hide()
    im, t = grab(); out = os.path.join(ROOT, 'capture', name + '.png'); im.save(out); print(out, t, im.size)
O_WR, O_RD, O_CMD = 4, 8, 12
O_FRAME = 12 + 256 * 16; O_BUSY = O_FRAME + 4; O_HWND = O_BUSY + 4; SHM = O_HWND + 4
VK = {'esc': 0x1b, 'enter': 0x0d, 'space': 0x20, 'tab': 0x09, 'back': 0x08, 'up': 0x26, 'down': 0x28, 'left': 0x25,
      'right': 0x27, 'shift': 0x10, 'ctrl': 0x11, 'alt': 0x12, 'pgup': 0x21, 'pgdn': 0x22, 'home': 0x24, 'end': 0x23, 'del': 0x2e}
for i in range(12): VK['f%d' % (i + 1)] = 0x70 + i
for c in 'abcdefghijklmnopqrstuvwxyz': VK[c] = ord(c.upper())
for c in '0123456789': VK[c] = ord(c)
def shm():
    import mmap, struct
    try: m = mmap.mmap(-1, SHM, tagname='H4JP_TESTIO')
    except Exception: raise SystemExit('no shm')
    if struct.unpack_from('<I', m, 0)[0] != 0x504a3448: raise SystemExit('game (test DLL) not running')
    return m
def push(m, *cmds):
    import struct
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<ii', m, O_WR)
            if wr - rd < 250: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, O_CMD + (wr & 255) * 16, *c)
        struct.pack_into('<i', m, O_WR, wr + 1)
def settle(m, timeout=30):
    import struct
    t = time.time()
    while time.time() - t < timeout:
        wr, rd = struct.unpack_from('<ii', m, O_WR); busy = struct.unpack_from('<i', m, O_BUSY)[0]
        if wr == rd and not busy: return
        time.sleep(0.03)
def key(names, hold=60, gap=150):
    m = shm()
    for n in names:
        if n.startswith('wait:'): push(m, (6, int(float(n[5:]) * 1000))); continue
        if n.startswith('ch:'):
            for ch in n[3:]: push(m, (7, ord(ch)), (6, 30))
            continue
        code = VK[n.lower()] if n.lower() in VK else int(n, 0)
        push(m, (1, code), (6, hold), (2, code), (6, gap))
    settle(m)
def move(x, y):
    m = shm(); push(m, (3, x, y), (6, 80)); settle(m)
def click(x, y, right=False, double=False):
    m = shm(); r = 1 if right else 0
    push(m, (3, x, y), (6, 80), (4, r), (6, 80), (5, r), (6, 60 if double else 150))
    if double: push(m, (9, r), (6, 60), (5, r), (6, 150))
    settle(m)
def start(variant):
    dst = os.path.join(GAME, 'wsock32.dll')
    if os.path.exists(dst): os.remove(dst)
    if variant == 'jp': shutil.copy(os.path.join(ROOT, 'build', 'wsock32.dll'), dst)
    env = dict(os.environ); env['H4JP_TEST'] = '1'; env['H4JP_LOG'] = os.path.join(ROOT, 'test', 'log.txt')
    p = subprocess.Popen([os.path.join(GAME, 'heroes4.exe')], cwd=GAME, env=env)
    open(PIDF, 'w').write(str(p.pid))
    for i in range(100):
        time.sleep(0.3)
        try:
            h, t = find_window(p.pid)
            if t in ('Heroes 4', 'H4JP TEST'):
                u32.SetWindowPos(h, 1, 2700, 100, 0, 0, 0x15); print('pid', p.pid, t); return
        except SystemExit: pass
    print('pid', p.pid, 'window not found yet')
def stop():
    try: subprocess.call(['taskkill', '/PID', open(PIDF).read().strip(), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e: print(e)
    time.sleep(0.8)
    dst = os.path.join(GAME, 'wsock32.dll')
    if os.path.exists(dst): os.remove(dst)
if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'start': start(sys.argv[2] if len(sys.argv) > 2 else 'none')
    elif c == 'shot': shot(sys.argv[2])
    elif c == 'key': key(sys.argv[2:])
    elif c == 'click': click(int(sys.argv[2]), int(sys.argv[3]), 'right' in sys.argv[4:], 'double' in sys.argv[4:])
    elif c == 'move': move(int(sys.argv[2]), int(sys.argv[3]))
    elif c == 'stop': stop()
