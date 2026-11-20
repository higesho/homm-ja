# Test harness for the running game (windowed). Never brings the window to the front.
#   python tools/win.py start | stop | shot [name] | click X Y | rclick X Y | key VK | text STR | wait SEC
#   coordinates are client pixels (1440x900 by default)
import os, sys, time, subprocess, ctypes, ctypes.wintypes as W
u = ctypes.windll.user32; u.SetProcessDPIAware()
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
GAME = os.path.dirname(ROOT)
TITLE = 'Might & Magic Heroes VI'

def hwnd():
    return u.FindWindowW(None, TITLE)

def lp(x, y): return (int(y) << 16) | (int(x) & 0xffff)

def click(x, y, down=0x201, up=0x202, wp=1):
    h = hwnd()
    u.PostMessageW(h, 0x200, 0, lp(x, y)); time.sleep(0.05)
    u.PostMessageW(h, down, wp, lp(x, y)); time.sleep(0.08)
    u.PostMessageW(h, up, 0, lp(x, y))

def key(vk):
    h = hwnd()
    u.PostMessageW(h, 0x100, vk, 1); time.sleep(0.05); u.PostMessageW(h, 0x101, vk, 0xC0000001)

def main(a):
    c = a[0]
    if c == 'start':
        subprocess.Popen([os.path.join(GAME, 'Might & Magic Heroes VI.exe')], cwd=GAME)
        for _ in range(180):
            time.sleep(1)
            if hwnd(): print('window up'); return
        print('no window')
    elif c == 'stop':
        subprocess.run(['taskkill', '/f', '/im', 'Might & Magic Heroes VI.exe'], capture_output=True)
        for _ in range(30):
            if not hwnd(): print('stopped'); return
            time.sleep(1)
    elif c == 'shot':
        name = a[1] if len(a) > 1 else 'shot'
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'capture.py'), os.path.join(ROOT, 'shots', name + '.png')])
    elif c == 'click': click(a[1], a[2])
    elif c == 'rclick': click(a[1], a[2], 0x204, 0x205, 2)
    elif c == 'key': key(int(a[1], 0))
    elif c == 'text':
        for ch in a[1]: u.PostMessageW(hwnd(), 0x102, ord(ch), 1); time.sleep(0.03)
    elif c == 'wait': time.sleep(float(a[1]))

if __name__ == '__main__':
    # several commands in one call:  win.py click 10 20 , wait 1 , shot x
    cmd = [];
    for t in sys.argv[1:] + [',']:
        if t == ',':
            if cmd: main(cmd)
            cmd = []
        else: cmd.append(t)
