# scripted steps for the HoMM2 test window
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
def wait(s): time.sleep(s)
def to_menu():
    for i in range(4): win.key(['esc']); wait(2)
    win.move(320, 240, cal=True); win.click(320, 240); wait(4)
def load_tutorial():
    win.click(237, 215); wait(3)      # Load Game
    win.click(525, 72); wait(3)       # Standard
    win.click(525, 72); wait(3)       # Original map
    win.click(390, 125); wait(0.6); win.click(300, 384); wait(9)
def tutorial_fight():
    win.click(600, 112); wait(1.5)
    win.click(300, 312); wait(0.7); win.click(300, 312); wait(6)
    win.click(335, 333); wait(0.8); win.click(335, 333); wait(6)
def new_game():
    win.click(520, 230); wait(2); win.click(525, 72); wait(3); win.click(525, 72); wait(4)
    win.click(285, 376); wait(8); win.click(533, 66); wait(1.5)
if __name__ == '__main__':
    for step in sys.argv[1:]: globals()[step]()
