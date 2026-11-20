# test steps: python tools/h4.py menu | scenario | play  (game started with win.py start jp)
import sys, time, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
def menu():
    time.sleep(8); win.key(['esc', 'wait:2', 'esc', 'wait:2', 'esc']); time.sleep(3)
def scenario():
    win.click(905, 110); time.sleep(2); win.click(660, 185); time.sleep(4)
def play():
    win.click(800, 643); time.sleep(5); win.click(808, 645); time.sleep(15)
if __name__ == '__main__':
    for a in sys.argv[1:]: globals()[a]()
