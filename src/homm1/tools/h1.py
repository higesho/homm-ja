# scripted steps for the HoMM1 test window (verify each step on screen when it matters)
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
def wait(s): time.sleep(s)
def to_menu():
    win.key(['esc']); wait(3); win.key(['esc']); wait(3)
    win.move(320, 240, cal=True); wait(0.5)
def new_game():
    win.key(['n']); wait(2); win.key(['s']); wait(4); win.shot('ng_setup'); win.key(['enter']); wait(45); win.shot('ng_start')
if __name__ == '__main__':
    for step in sys.argv[1:]: globals()[step]()
