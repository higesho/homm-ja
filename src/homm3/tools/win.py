# Drive the test game (H3JP_TEST=1) through shared memory; the game window is identified by its
# unique title "H3JP TEST" (never the foreground window).
#   python tools/win.py start | stop | shot NAME | key K.. | click X Y [r] | move X Y | wait MS | type TEXT
import sys, os, time, mmap, struct, subprocess, shutil, ctypes
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.environ.get('H3JP_GAME') or os.path.dirname(ROOT)
EXE = os.environ.get('H3JP_EXE') or 'Heroes3.exe'
TITLE = 'H3JP TEST'
O_WR, O_RD, O_CMD = 4, 8, 12
O_REQ = 12 + 256 * 16; O_DONE = O_REQ + 4; O_PATH = O_DONE + 4; O_FRAME = O_PATH + 260; O_BUSY = O_FRAME + 4; O_HWND = O_BUSY + 4
SHM = O_HWND + 4
VK = {'esc': 0x1b, 'enter': 0x0d, 'space': 0x20, 'tab': 0x09, 'back': 0x08, 'up': 0x26, 'down': 0x28, 'left': 0x25,
      'right': 0x27, 'shift': 0x10, 'ctrl': 0x11, 'alt': 0x12, 'pgup': 0x21, 'pgdn': 0x22, 'home': 0x24, 'end': 0x23, 'del': 0x2e}
for i in range(12): VK['f%d' % (i + 1)] = 0x70 + i
for c in 'abcdefghijklmnopqrstuvwxyz': VK[c] = ord(c.upper())
for c in '0123456789': VK[c] = ord(c)

def hwnd(): return ctypes.windll.user32.FindWindowA(None, TITLE.encode())
def shm():
    try: m = mmap.mmap(-1, SHM, tagname='H3JP_TESTIO')
    except Exception: raise SystemExit('no shm')
    if struct.unpack_from('<I', m, 0)[0] != 0x504a3348: raise SystemExit('game (test DLL) not running')
    return m
def push(m, *cmds):
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<ii', m, O_WR)
            if wr - rd < 250: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, O_CMD + (wr & 255) * 16, *c)
        struct.pack_into('<i', m, O_WR, wr + 1)
def settle(m, timeout=30):
    t = time.time()
    while time.time() - t < timeout:
        wr, rd = struct.unpack_from('<ii', m, O_WR); busy = struct.unpack_from('<i', m, O_BUSY)[0]
        if wr == rd and not busy: return
        time.sleep(0.03)
def key(m, k, hold=50, gap=120):
    code = VK[k] if k in VK else int(k, 0)
    push(m, (1, code), (6, hold), (2, code), (6, gap))
def click(m, x, y, right=0, hold=80, gap=150):
    push(m, (3, x, y), (6, 60), (4, right), (6, hold), (5, right), (6, gap))
def shot(name, scale=None):
    from PIL import Image
    m = shm(); settle(m)
    if False:  # PrintWindow of the DDrawCompat presentation window, cropped to the game's client area
        sys.path.insert(0, os.path.dirname(__file__)); from pw import grab
        from ctypes import wintypes
        u = ctypes.windll.user32; h = hwnd()
        wr = wintypes.RECT(); u.GetWindowRect(h, ctypes.byref(wr))
        pt = wintypes.POINT(0, 0); u.ClientToScreen(h, ctypes.byref(pt))
        cr = wintypes.RECT(); u.GetClientRect(h, ctypes.byref(cr))
        im, ok = grab('[DDrawCompat] ' + TITLE)
        pr = wintypes.RECT(); u.GetWindowRect(u.FindWindowA(None, ('[DDrawCompat] ' + TITLE).encode()), ctypes.byref(pr))
        x0, y0 = pt.x - pr.left, pt.y - pr.top
        im = im.crop((x0, y0, x0 + cr.right, y0 + cr.bottom))
        os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
        out = os.path.join(ROOT, 'capture', name + '.png'); im.save(out); print(out, im.size); return
    os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
    bmp = os.path.join(ROOT, 'work', 'shot.bmp')
    if os.path.exists(bmp): os.remove(bmp)
    p = bmp.encode('mbcs') + b'\0'; m[O_PATH:O_PATH + len(p)] = p
    req = struct.unpack_from('<i', m, O_REQ)[0] + 1; struct.pack_into('<i', m, O_REQ, req)
    t = time.time()
    while struct.unpack_from('<i', m, O_DONE)[0] != req:
        if time.time() - t > 15: raise SystemExit('shot timeout')
        time.sleep(0.05)
    im = Image.open(bmp).convert('RGB')
    out = os.path.join(ROOT, 'capture', name + '.png'); im.save(out); print(out, im.size)
def start():
    stop(quiet=True)
    shutil.copy(os.path.join(ROOT, 'build', 'test', 'wsock32.dll'), os.path.join(GAME, 'wsock32.dll'))   # 試験版（src/build.bat test で作る）
    env = dict(os.environ, H3JP_TEST=os.environ.get('H3JP_TEST','1'), H3JP_LOG=os.path.join(ROOT, 'work', 'h3jp.log'))
    p = subprocess.Popen([os.path.join(GAME, EXE)], cwd=GAME, env=env)
    open(os.path.join(ROOT, 'work', 'pid.txt'), 'w').write(str(p.pid))
    for i in range(100):
        time.sleep(0.2)
        if hwnd(): break
    print('started', p.pid, 'hwnd', hwnd())
def stop(quiet=False):
    pf = os.path.join(ROOT, 'work', 'pid.txt')
    if os.path.exists(pf):
        subprocess.run(['taskkill', '/F', '/PID', open(pf).read().strip()], capture_output=True)
        os.remove(pf); time.sleep(0.5)
    dll = os.path.join(GAME, 'wsock32.dll')
    for i in range(20):
        try:
            if os.path.exists(dll): os.remove(dll)
            break
        except PermissionError: time.sleep(0.3)
    if not quiet: print('stopped')
if __name__ == '__main__':
    a = sys.argv[1:]
    c = a[0]
    if c == 'start': start()
    elif c == 'stop': stop()
    elif c == 'shot': shot(a[1])
    else:
        m = shm()
        if c == 'key':
            for k in a[1:]: key(m, k)
        elif c == 'click': click(m, int(a[1]), int(a[2]), 1 if len(a) > 3 and a[3] == 'r' else 0)
        elif c == 'rdown': push(m, (3, int(a[1]), int(a[2])), (6, 60), (4, 1), (6, 400))
        elif c == 'rup': push(m, (5, 1), (6, 200))
        elif c == 'move': push(m, (3, int(a[1]), int(a[2])), (6, 100))
        elif c == 'wait': push(m, (6, int(a[1])))
        elif c == 'type':
            for ch in ' '.join(a[1:]): push(m, (7, ord(ch)), (6, 30))
        settle(m)
