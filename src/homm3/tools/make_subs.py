# build/subs.bin: Japanese movie subtitles (text/subs/subs_ja.json) for src/subs.c
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from make_data import euc
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Bink movies in VIDEO.VID identified by width, height, frame count
VIDS = {'H3Intro': (640, 360, 2426), 'Endgame': (640, 360, 1261)}
MAXC = 30   # full-width chars per line (18px glyphs in a 640px frame)
NOSTART = set('、。，．・ー〜！？」』）ぁぃぅぇぉっゃゅょァィゥェォッャュョ…')
NOEND = set('「『（')
def fw(s):
    return ''.join(chr(ord(c) + 0xFEE0) if '!' <= c <= '~' else ('\u3000' if c == ' ' else c) for c in s)
def wrap(s):
    s = fw(s.strip())
    if '\n' in s or len(s) <= MAXC: return s
    best = None
    for i in range(1, len(s)):
        if s[i] in NOSTART or s[i - 1] in NOEND: continue
        if i > MAXC or len(s) - i > MAXC: continue
        kata = lambda c: '゠' <= c <= 'ヿ'
        if kata(s[i - 1]) and kata(s[i]): continue          # never split a katakana word
        score = abs(len(s) / 2 - i) - (8 if s[i - 1] in '、。' else 3 if s[i - 1] in 'のをにはがともでへ' else 0)
        if best is None or score < best[0]: best = (score, i)
    if not best: raise SystemExit('cannot wrap: ' + s)
    return s[:best[1]] + '\n' + s[best[1]:]
def build():
    ja = json.load(open(os.path.join(ROOT, 'text', 'subs', 'subs_ja.json'), encoding='utf-8'))
    hdr = b'H3SB' + struct.pack('<I', len(VIDS)); base = 8 + 20 * len(VIDS)
    tabs = b''; strs = b''; vh = b''; ntot = 0
    cues_all = []
    for name, (w, h, fr) in VIDS.items():
        cues = ja[name]; L = []
        for k, (s, e, t) in enumerate(cues):
            st = max(0.0, s - 0.15)
            en = max(e + 0.7, s + 2.0)
            if k + 1 < len(cues): en = min(en, cues[k + 1][0] - 0.2)
            L.append((int(st * 1000), int(en * 1000), wrap(t)))
        cues_all.append((w, h, fr, L))
    off_tabs = base; ntot = sum(len(c[3]) for c in cues_all); off_str = off_tabs + 12 * ntot
    for w, h, fr, L in cues_all:
        vh += struct.pack('<IIIII', w, h, fr, len(L), off_tabs + len(tabs))
        for s, e, t in L:
            tabs += struct.pack('<III', s, e, off_str + len(strs)); strs += euc(t) + b'\0'
            print(f'{s/1000:7.2f}-{e/1000:7.2f}', t.replace('\n', ' / '))
    return hdr + vh + tabs + strs
if __name__ == '__main__':
    b = build(); open(os.path.join(ROOT, 'build', 'subs.bin'), 'wb').write(b); print('subs.bin', len(b))
