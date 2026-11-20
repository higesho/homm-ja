# Test harness for Heroes VII (windowed 1440x900). Never brings the window to the front.
#   python tools/win.py install | uninstall | start | stop | shot [name] | click X Y | rclick X Y | move X Y | wheel N | key VK | text STR | wait SEC | title | counters
#   mouse goes through the test-only dinput8.dll (src/h7test.c); keys are posted window messages
#   several commands:  win.py click 10 20 , wait 1 , shot x
#   start launches the exe directly (steam_appid.txt makes steam_api accept it) with a 1440x900 window;
#   the user's own config is backed up by 'install' and restored by 'uninstall'.
import os, sys, re, stat, time, shutil, subprocess, ctypes, ctypes.wintypes as W
u = ctypes.windll.user32; u.SetProcessDPIAware()
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
GAME = os.path.dirname(ROOT)
BIN = os.path.join(GAME, 'Binaries', 'Win64')
EXE = 'MMH7Game-Win64-Shipping.exe'
LOC = os.path.join(GAME, 'MMH7Game', 'Localization')
CFG = os.path.join(os.environ['USERPROFILE'], 'Documents', 'My Games', 'Might & Magic Heroes VII', 'MMH7Game', 'Config')
BAK = os.path.join(ROOT, 'work', 'cfgbak')
STAGE = os.path.join(ROOT, 'work', 'stage')   # built JPN folders: <area>/JPN/*.JPN
AREAS = ['General', 'Content', 'Maps', 'Duel']

def hwnd():
    found = []
    pid_of = {}
    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, l):
        if not u.IsWindowVisible(h): return True
        buf = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, buf, 256)
        if buf.value.startswith('MMH7 Game'):
            found.append(h)
        return True
    u.EnumWindows(cb, 0)
    return found[0] if found else 0

def lp(x, y): return (int(y) << 16) | (int(x) & 0xffff)

# ---- input through the test DLL (build/dinput8.dll, shared memory H7JP_TESTIO) ----
class Shm(ctypes.Structure):
    _fields_ = [('magic', W.LONG), ('wr', W.LONG), ('rd', W.LONG), ('cmd', (W.LONG * 4) * 256),
                ('x', W.LONG), ('y', W.LONG), ('busy', W.LONG), ('counters', W.LONG * 8)]
_shm = None
def shm():
    global _shm
    if _shm is None:
        k = ctypes.windll.kernel32
        k.OpenFileMappingA.restype = ctypes.c_void_p
        k.MapViewOfFile.restype = ctypes.c_void_p
        k.MapViewOfFile.argtypes = [ctypes.c_void_p, W.DWORD, W.DWORD, W.DWORD, ctypes.c_size_t]
        m = k.OpenFileMappingA(0xF001F, False, b'H7JP_TESTIO')
        if not m: raise SystemExit('test DLL not loaded (no H7JP_TESTIO)')
        _shm = Shm.from_address(k.MapViewOfFile(m, 0xF001F, 0, 0, ctypes.sizeof(Shm)))
    return _shm
C_KEYDOWN, C_KEYUP, C_MOVETO, C_BDOWN, C_BUP, C_WAIT, C_WHEEL = range(1, 8)
def push(t, a=0, b=0):
    s = shm()
    i = s.wr & 255
    s.cmd[i][0], s.cmd[i][1], s.cmd[i][2], s.cmd[i][3] = t, int(a), int(b), 0
    s.wr += 1
def drain(timeout=10):
    s = shm(); t = time.time()
    while (s.rd != s.wr or s.busy) and time.time() - t < timeout: time.sleep(0.02)

def click(x, y, btn=0):
    push(C_MOVETO, x, y); push(C_WAIT, 120)
    push(C_BDOWN, btn, 1); push(C_WAIT, 90); push(C_BUP, btn, 1); push(C_WAIT, 60)
    drain()

def move(x, y):
    push(C_MOVETO, x, y); push(C_WAIT, 100); drain()

def key(vk):
    h = hwnd()
    sc = u.MapVirtualKeyW(vk, 0)
    u.PostMessageW(h, 0x100, vk, 1 | (sc << 16)); time.sleep(0.05); u.PostMessageW(h, 0x101, vk, 0xC0000001 | (sc << 16))

def install():
    if not os.path.isdir(BAK) or not os.listdir(BAK):   # keep the first (pristine) backup
        os.makedirs(BAK, exist_ok=True)
        for f in os.listdir(CFG):
            shutil.copy2(os.path.join(CFG, f), os.path.join(BAK, f))
    open(os.path.join(BIN, 'steam_appid.txt'), 'w').write('321960')
    shutil.copy2(os.path.join(ROOT, 'build', 'test', 'dinput8.dll'), os.path.join(BIN, 'dinput8.dll'))
    rd = os.path.join(ROOT, 'build', 'h7test_redirect.txt')
    if os.path.exists(rd): shutil.copy2(rd, os.path.join(BIN, 'h7test_redirect.txt'))
    elif os.path.exists(os.path.join(BIN, 'h7test_redirect.txt')): os.remove(os.path.join(BIN, 'h7test_redirect.txt'))
    for a in AREAS:
        s = os.path.join(STAGE, a, 'JPN'); d = os.path.join(LOC, a, 'JPN')
        if os.path.exists(d): shutil.rmtree(d)
        if os.path.exists(s): shutil.copytree(s, d)
    s = os.path.join(STAGE, 'CookedPC', 'H7JP'); d = os.path.join(GAME, 'MMH7Game', 'CookedPC', 'H7JP')
    if os.path.exists(d): shutil.rmtree(d)
    if os.path.exists(s): shutil.copytree(s, d)
    print('installed')

# UCP test install: UCP (work/ucp/x) and our UCP JPN files (work/stage_ucp) into Documents\My Games\...\MMH7Game,
# where UCP itself goes. Those folders do not exist there otherwise, so uninstall removes them whole.
USERGAME = os.path.dirname(CFG)
def ucp_install(jpn=True):
    ucp_uninstall()
    shutil.copytree(os.path.join(ROOT, 'work', 'ucp', 'x', 'CookedPC'), os.path.join(USERGAME, 'CookedPC'))
    shutil.copytree(os.path.join(ROOT, 'work', 'ucp', 'x', 'Localization'), os.path.join(USERGAME, 'Localization'))
    if jpn:
        shutil.copytree(os.path.join(ROOT, 'work', 'stage_ucp', 'Localization'), os.path.join(USERGAME, 'Localization'), dirs_exist_ok=True)
    print('UCP installed', 'with JPN' if jpn else 'without JPN')

def is_junction(p):
    f = ctypes.windll.kernel32.GetFileAttributesW
    f.restype = W.DWORD; f.argtypes = [W.LPCWSTR]
    a = f(p)
    return a != 0xFFFFFFFF and (a & 0x400) != 0          # FILE_ATTRIBUTE_REPARSE_POINT

def ucp_uninstall():
    """removes the mod folders from Documents\\My Games\\...\\MMH7Game (junctions only unlinked, never followed)"""
    for n in ('CookedPC', 'Localization', 'CustomMaps'):
        d = os.path.join(USERGAME, n)
        if is_junction(d): os.rmdir(d)
        elif os.path.exists(d): shutil.rmtree(d)

# Heroes 7.5 Mod test install: CookedPC and CustomMaps as junctions to work/h75/full (28 GB, not copied),
# Localization copied from the mod plus our H7.5 JPN files (work/stage_h75)
def h75_install(jpn=True):
    ucp_uninstall()
    F = os.path.join(ROOT, 'work', 'h75', 'full')
    for n in ('CookedPC', 'CustomMaps'):
        subprocess.run(['cmd', '/c', 'mklink', '/J', os.path.join(USERGAME, n), os.path.join(F, n)], capture_output=True)
        if not is_junction(os.path.join(USERGAME, n)): raise SystemExit('junction failed: ' + n)
    shutil.copytree(os.path.join(F, 'Localization'), os.path.join(USERGAME, 'Localization'))
    s = os.path.join(ROOT, 'work', 'stage_h75', 'Localization')
    if jpn and os.path.exists(s): shutil.copytree(s, os.path.join(USERGAME, 'Localization'), dirs_exist_ok=True)
    # what the H7.5 installer does to the user ini: 9 army slots, file made read-only (undone by uninstall)
    ini = os.path.join(CFG, 'MMH7Game.ini')
    t = open(ini, encoding='utf-8', errors='surrogateescape').read()
    t = re.sub(r'(?m)^cMaxArmySize=.*$', 'cMaxArmySize=9', t)
    open(ini, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(t)
    os.chmod(ini, 0o444)
    print('H7.5 installed', 'with JPN' if jpn and os.path.exists(s) else 'without JPN')

def uninstall():
    for f in ['dinput8.dll', 'steam_appid.txt', 'h7test_log.txt', 'h7test_redirect.txt']:
        p = os.path.join(BIN, f)
        if os.path.exists(p): os.remove(p)
    for a in AREAS:
        d = os.path.join(LOC, a, 'JPN')
        if os.path.exists(d): shutil.rmtree(d)
    d = os.path.join(GAME, 'MMH7Game', 'CookedPC', 'H7JP')
    if os.path.exists(d): shutil.rmtree(d)
    ucp_uninstall()
    for f in os.listdir(BAK):
        d = os.path.join(CFG, f)
        if os.path.exists(d): os.chmod(d, stat.S_IWRITE | stat.S_IREAD)
        shutil.copy2(os.path.join(BAK, f), d)
    print('uninstalled, config restored')

def main(a):
    c = a[0]
    if c == 'install': install()
    elif c == 'uninstall': uninstall()
    elif c == 'ucp': ucp_install(jpn=not (len(a) > 1 and a[1] == 'nojpn'))
    elif c == 'noucp': ucp_uninstall()
    elif c == 'h75': h75_install(jpn=not (len(a) > 1 and a[1] == 'nojpn'))
    elif c == 'start':
        args = [os.path.join(BIN, EXE), '-uplay_steam_mode', '-windowed', '-ResX=1440', '-ResY=900', '-nostartupmovies'] + a[1:]
        if 'movies' in args: args.remove('movies'); args.remove('-nostartupmovies')
        subprocess.Popen(args, cwd=BIN)
        for _ in range(240):
            time.sleep(1)
            if hwnd(): print('window up'); return
        print('no window')
    elif c == 'stop':
        subprocess.run(['taskkill', '/f', '/im', EXE], capture_output=True)
        for _ in range(60):
            r = subprocess.run(['tasklist', '/fi', 'imagename eq ' + EXE], capture_output=True, text=True)
            if EXE[:20] not in r.stdout: print('stopped'); time.sleep(1); return
            time.sleep(1)
    elif c == 'title':
        h = hwnd(); buf = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, buf, 256)
        r = W.RECT(); u.GetClientRect(h, ctypes.byref(r)); print(h, buf.value, r.right, r.bottom)
    elif c == 'shot':
        name = a[1] if len(a) > 1 else 'shot'
        h = hwnd(); buf = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, buf, 256)
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'capture.py'), os.path.join(ROOT, 'shots', name + '.png'), buf.value])
    elif c == 'click': click(a[1], a[2])
    elif c == 'rclick': click(a[1], a[2], 1)
    elif c == 'move': move(a[1], a[2])
    elif c == 'wheel': push(C_WHEEL, a[1]); push(C_WAIT, 100); drain()
    elif c == 'counters': print(list(shm().counters), shm().x, shm().y)
    elif c == 'key': key(int(a[1], 0))
    elif c == 'text':
        for ch in a[1]: u.PostMessageW(hwnd(), 0x102, ord(ch), 1); time.sleep(0.03)
    elif c == 'wait': time.sleep(float(a[1]))

if __name__ == '__main__':
    cmd = []
    for t in sys.argv[1:] + [',']:
        if t == ',':
            if cmd: main(cmd)
            cmd = []
        else: cmd.append(t)
