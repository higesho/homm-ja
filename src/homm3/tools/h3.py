# test helpers on top of win.py
import sys, os, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from PIL import ImageStat
def m(): return win.shm()
def key(*ks):
    mm = m()
    for k in ks: win.key(mm, k)
    win.settle(mm)
def click(x, y, right=0):
    mm = m(); win.click(mm, x, y, right); win.settle(mm)
def shot(name): win.shot(name)
def img():
    import io, contextlib
    with contextlib.redirect_stdout(io.StringIO()): win.shot('_tmp')
    from PIL import Image
    return Image.open(os.path.join(win.ROOT, 'capture', '_tmp.png')).convert('RGB')
def is_menu(im):
    r, g, b = ImageStat.Stat(im.crop((520, 20, 780, 120))).mean
    return r > 110 and r > g + 30
def to_menu(start=True):
    if start: win.start(); time.sleep(4)
    for i in range(40):
        im = img()
        if is_menu(im): return True
        key('esc'); click(400, 300); time.sleep(0.7)
    raise SystemExit('main menu not reached')
if __name__ == '__main__':
    to_menu(start=len(sys.argv) < 2 or sys.argv[1] != 'nostart'); print('at main menu')

def rpop(x, y, name):
    """hold right button on (x,y), take a shot, release"""
    mm = m(); win.push(mm, (3, x, y), (6, 60), (4, 1), (6, 500)); win.settle(mm)
    win.shot(name)
    win.push(mm, (5, 1), (6, 200)); win.settle(mm)
