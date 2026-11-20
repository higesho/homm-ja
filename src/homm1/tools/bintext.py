# label texts inside the window layout files (*.BIN) of HEROES2.AGG / HEROES2X.AGG:
#   ... u16 len, text, NUL, font name ("bigfont.fnt"/"smalfont.fnt"), NUL ...
import os, sys, re, json, struct
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from agg import agg
AGGS = ['HEROES.AGG']
PAT = re.compile(rb'([\x01-\xff][\x00])([\x20-\x7e\n]*)\x00((?:big|smal)font\.fnt)\x00')

def labels(data):
    """[(len_offset, text)] of the text widgets of one BIN"""
    out = []
    for m in PAT.finditer(data):
        n = struct.unpack('<H', m.group(1))[0]
        t = m.group(2)
        if n == len(t) + 1 and re.search(rb'[A-Za-z]{2}', t):
            out.append((m.start(), t.decode('latin-1')))
    return out

def all_labels():
    res = []
    for a in AGGS:
        A = agg(os.path.join(ROOT, '..', 'DATA', a))
        for k in sorted(A):
            if not k.endswith('.BIN'): continue
            for off, t in labels(A[k]): res.append((a, k, off, t))
    return res

if __name__ == '__main__':
    L = all_labels()
    u = sorted(set(t for a, k, o, t in L))
    print(len(L), 'labels', len(u), 'unique')
    print(u)
