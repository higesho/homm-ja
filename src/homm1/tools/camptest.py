# Campaign start test: start campaign 1 (Lord Ironfist) N times and save the hero info panels side by side.
# The starting hero is random, so several runs are needed to meet a hero with a long name.
#   python tools/camptest.py [jp|none] [runs] [tag]     -> capture/<tag>_panel.png, capture/<tag><k>.png
#   BISECT=lo,hi python tools/make_patch.py  builds a blob that translates only strings [lo, hi) (bisecting)
import sys, os, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import win

def run(tag, variant='jp'):
    from PIL import Image
    win.start(variant); time.sleep(25)
    win.key(['esc']); time.sleep(3); win.key(['esc']); time.sleep(4)
    win.move(320, 240, cal=True); time.sleep(1)          # without this the cursor position is unknown
    win.tclick(497, 105); time.sleep(3)                  # New Game
    win.tclick(497, 172); time.sleep(4)                  # Campaign Game
    win.tclick(480, 105); time.sleep(25)                 # Play Lord Ironfist
    win.tclick(316, 342); time.sleep(60)                 # Okay
    win.shot(tag)
    im = Image.open(os.path.join(ROOT, 'capture', tag + '.png')).convert('RGB')
    if im.size != (640, 480): im = im.resize((640, 480))
    win.stop()
    return im

if __name__ == '__main__':
    from PIL import Image
    variant = sys.argv[1] if len(sys.argv) > 1 else 'jp'
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    tag = sys.argv[3] if len(sys.argv) > 3 else 'camp_' + variant
    out = Image.new('RGB', (480 * n, 270))
    for k in range(n):
        im = run('%s%d' % (tag, k), variant)
        out.paste(im.crop((478, 390, 638, 480)).resize((480, 270), Image.NEAREST), (480 * k, 0))
        out.save(os.path.join(ROOT, 'capture', tag + '_panel.png'))
    print('done')
