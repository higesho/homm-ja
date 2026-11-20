# Drive the HoMM2 test DOSBox by PID (never the foreground window). Game coords = 640x480.
#   python tools/win.py start [jp|none] | shot NAME | key K.. | click X Y [right] | move X Y | stop
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
            if b.value.startswith('DOSBox'): found.append((h, b.value))
        return True
    u32.EnumWindows(WNDENUMPROC(cb), 0)
    if not found: raise SystemExit('DOSBox window of pid %d not found' % pid)
    return found[0]
def client_size(h):
    if u32.IsIconic(h): u32.ShowWindow(h, 4); time.sleep(0.5)
    r = W.RECT(); u32.GetClientRect(h, ctypes.byref(r)); return r.right, r.bottom
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
    im, t = grab()
    if im.size != (640, 480): im = im.resize((640, 480))
    out = os.path.join(ROOT, 'capture', name + '.png'); im.save(out); print(out, t)
VK = {'enter': (0x0d, 0x1c), 'esc': (0x1b, 0x01), 'space': (0x20, 0x39), 'up': (0x26, 0x48), 'down': (0x28, 0x50),
      'left': (0x25, 0x4b), 'right': (0x27, 0x4d), 'tab': (0x09, 0x0f), 'back': (0x08, 0x0e)}
for i in range(1, 11): VK['f%d' % i] = (0x6f + i, 0x3a + i)
for r, s0 in zip(['1234567890', 'qwertyuiop', 'asdfghjkl', 'zxcvbnm'], [0x02, 0x10, 0x1e, 0x2c]):
    for k, ch in enumerate(r): VK[ch] = (ord(ch.upper()), s0 + k)
def post_key(h, name, hold=0.06):
    vk, sc = VK[name]; ext = 1 << 24 if name in ('up', 'down', 'left', 'right') else 0
    u32.PostMessageW(h, 0x0100, vk, 1 | (sc << 16) | ext); time.sleep(hold)
    u32.PostMessageW(h, 0x0101, vk, 1 | (sc << 16) | ext | (3 << 30)); time.sleep(0.1)
def key(names):
    h, t = find_window()
    for n in names:
        if n.startswith('text:'):
            for ch in n[5:]: post_key(h, 'space' if ch == ' ' else ch.lower())
        elif n.startswith('wait:'): time.sleep(float(n[5:]))
        else: post_key(h, n.lower())
def lp_of(h, x, y):
    w, hh = client_size(h); return (int(y * hh / 480) << 16) | int(x * w / 640)
def move(x, y, cal=False):
    # the game moves its cursor by the relative motion; 'cal' pushes it into the corners first (scrolls the adventure map!)
    h, t = find_window()
    for k in range(3 if cal else 0):
        u32.PostMessageW(h, 0x0200, 0, lp_of(h, 639, 479)); time.sleep(0.04)
        for j in range(1, 33):                      # sweep to the corner in small steps
            u32.PostMessageW(h, 0x0200, 0, lp_of(h, 639 - 639 * j // 32, 479 - 479 * j // 32)); time.sleep(0.015)
    # step there: big jumps of relative motion are not taken by HoMM1
    try: cx, cy = map(int, open(PIDF + '.pos').read().split())
    except Exception: cx, cy = 320, 240
    if cal: cx, cy = 0, 0
    n = max(1, int(max(abs(x - cx), abs(y - cy)) / 20))
    for k in range(1, n + 1):
        u32.PostMessageW(h, 0x0200, 0, lp_of(h, cx + (x - cx) * k // n, cy + (y - cy) * k // n)); time.sleep(0.02)
    open(PIDF + '.pos', 'w').write('%d %d' % (x, y))
    time.sleep(0.15)
    return h
def click(x, y, right=False):
    h = move(x, y); lp = lp_of(h, x, y)
    down, up, mk = (0x0204, 0x0205, 2) if right else (0x0201, 0x0202, 1)
    u32.PostMessageW(h, down, mk, lp); time.sleep(0.12); u32.PostMessageW(h, up, 0, lp); time.sleep(0.15)
def start(variant):
    dst = os.path.join(GAME, 'DOSBOX', 'winmm.dll')
    if os.path.exists(dst): os.remove(dst)
    if variant == 'jp': shutil.copy(os.path.join(ROOT, 'build', 'winmm.dll'), dst)
    dbx = os.path.join(GAME, 'DOSBOX')
    p = subprocess.Popen([os.path.join(dbx, 'DOSBox.exe'), '-conf', os.path.join(ROOT, 'test', os.environ.get('H2CONF', 'test.conf')), '-noconsole'], cwd=dbx)
    open(PIDF, 'w').write(str(p.pid))
    for i in range(50):
        time.sleep(0.2)
        try:
            h, t = find_window(p.pid); u32.SetWindowPos(h, 1, 1300, 500, 0, 0, 0x15); print('pid', p.pid, t); return
        except SystemExit: pass
    print('pid', p.pid, 'window not found yet')
def stop():
    try: subprocess.call(['taskkill', '/PID', open(PIDF).read().strip(), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e: print(e)
    dst = os.path.join(GAME, 'DOSBOX', 'winmm.dll')
    time.sleep(0.5)
    if os.path.exists(dst): os.remove(dst)
if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'start': start(sys.argv[2] if len(sys.argv) > 2 else 'none')
    elif c == 'shot': shot(sys.argv[2])
    elif c == 'key': key(sys.argv[2:])
    elif c == 'click': click(int(sys.argv[2]), int(sys.argv[3]), 'right' in sys.argv[4:])
    elif c == 'move': move(int(sys.argv[2]), int(sys.argv[3]), 'cal' in sys.argv[4:])
    elif c == 'stop': stop()

def _post_rel(h, dx, dy):
    """post a relative motion as small absolute steps around the window centre"""
    global _vx, _vy
    try: _vx
    except NameError: _vx, _vy = 320, 240
    n = max(1, int(max(abs(dx), abs(dy)) / 8))
    for k in range(1, n + 1):
        u32.PostMessageW(h, 0x0200, 0, lp_of(h, _vx + dx * k // n, _vy + dy * k // n)); time.sleep(0.012)
    _vx += dx; _vy += dy
    if not (40 < _vx < 600 and 40 < _vy < 440):            # re-centre the virtual position without motion
        pass
def find_cursor(h):
    """cursor tip by moving it a little and diffing two frames"""
    from PIL import ImageChops
    a, _ = grab(); _post_rel(h, 6, 6); time.sleep(0.12); b, _ = grab(); _post_rel(h, -6, -6); time.sleep(0.12)
    if a.size != (640, 480): a = a.resize((640, 480)); b = b.resize((640, 480))
    d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 40 else 0)
    bb = d.getbbox()
    return (bb[0], bb[1]) if bb else None
def goto(x, y, tries=8):
    h, t = find_window()
    for i in range(tries):
        p = find_cursor(h)
        if not p: _post_rel(h, 20, 20); continue
        dx, dy = x - p[0], y - p[1]
        if abs(dx) <= 2 and abs(dy) <= 2: return h
        _post_rel(h, dx, dy); time.sleep(0.1)
    return h
def gclick(x, y, right=False, double=False):
    h = goto(x, y); lp = lp_of(h, _vx, _vy)
    down, up, mk = (0x0204, 0x0205, 2) if right else (0x0201, 0x0202, 1)
    for k in range(2 if double else 1):
        u32.PostMessageW(h, down, mk, lp); time.sleep(0.1); u32.PostMessageW(h, up, 0, lp); time.sleep(0.12)

def find_cursor_tpl(im=None):
    """cursor tip (x, y) by the shape of the HoMM arrow (grey head with black outline)"""
    import numpy as np, json
    pts = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cursor_tpl.json')))
    if im is None:
        im, _ = grab()
    if im.size != (640, 480): im = im.resize((640, 480))
    a = np.array(im.convert('RGB')).astype(int)
    grey = (abs(a[..., 0] - a[..., 1]) < 20) & (abs(a[..., 1] - a[..., 2]) < 20) & (a[..., 0] > 100)
    dark = a.sum(axis=2) < 60
    H, W = grey.shape
    score = np.zeros((H - 10, W - 10), int)
    for x, y, g in pts:
        m = grey if g else dark
        score += m[y:y + H - 10, x:x + W - 10]
    yx = np.unravel_index(score.argmax(), score.shape)
    if score[yx] < len(pts) * 0.85: return None
    return int(yx[1]), int(yx[0])
def tgoto(x, y, tries=10):
    h, t = find_window()
    for i in range(tries):
        p = find_cursor_tpl()
        if not p: _post_rel(h, 15, 15); time.sleep(0.15); continue
        dx, dy = x - p[0], y - p[1]
        if abs(dx) <= 2 and abs(dy) <= 2: return h
        _post_rel(h, dx, dy); time.sleep(0.15)
    return h
def tclick(x, y, double=False, right=False):
    h = tgoto(x, y); lp = lp_of(h, _vx, _vy)
    down, up, mk = (0x0204, 0x0205, 2) if right else (0x0201, 0x0202, 1)
    for k in range(2 if double else 1):
        u32.PostMessageW(h, down, mk, lp); time.sleep(0.06); u32.PostMessageW(h, up, 0, lp); time.sleep(0.1)
    return h
