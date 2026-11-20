# all C strings of obj2 (data) with their fixup references
import sys, os, re, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from le import LE
L = LE(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'HEROES.EXE'))
B2 = L.objs[1]['base']
refs = {}
for (oi, so, st, fl, to, toff, pi) in L.fix:
    if to == 2 and toff is not None: refs.setdefault(toff, []).append((oi, so))
def all_strings():
    m = L.mem[1]; n = L.objs[1]['np'] * 4096
    out = []
    for mm in re.finditer(rb'[\x09\x0a\x0d\x20-\x7e]{2,}\x00', bytes(m[:n])):
        s, e = mm.start(), mm.end() - 1
        out.append((s, m[s:e].decode('latin-1')))
    return out
if __name__ == '__main__':
    S = all_strings()
    rows = []
    for off, s in S:
        r = refs.get(off, [])
        # refs into the middle of the string
        mid = sum(len(refs.get(k, [])) for k in range(off + 1, off + len(s)))
        rows.append(dict(off=off, s=s, nref=len(r), mid=mid))
    json.dump(rows, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'work', 'strings.json'), 'w'), indent=0)
    print(len(rows), sum(1 for r in rows if r['nref']), sum(1 for r in rows if r['mid']))
