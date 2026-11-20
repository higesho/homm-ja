#!/bin/sh
# start the test game and get to the main menu
cd "$(dirname "$0")/.."
python tools/win.py start
sleep 5
python tools/win.py key esc; sleep 1.5; python tools/win.py key esc; sleep 1.5; python tools/win.py click 400 300; sleep 2
