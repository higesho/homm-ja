# texts of the scenario / campaign map files
#   extract: text/src/maps.json  [{id, en, map, kind, limit}]
import os, sys, json, glob, struct
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import mp2
MAPS = os.path.join(ROOT, '..', 'MAPS')

def printable(t): return t and b'\0' not in t and all(32 <= c < 127 or c in (10, 13) for c in t)

def block_kind(b):
    """(kind, text offset) of an info block's message (fheroes2 layouts), or None"""
    n = len(b)
    if n < 9 or b[-1] != 0: return None
    ok = lambda o: printable(bytes(b[o:-1]))
    if all(x == 0 for x in b[:8]) and ok(8): return ('rumor', 8)
    if n > 50 and b[0] == 0 and b[42] == 1 and ok(49): return ('daily', 49)
    if n > 138 and b[0] == 0 and ok(138): return ('sphinx', 138)
    if b[0] == 1 and ok(9): return ('sign', 9)
    if n > 49 and b[0] == 1 and ok(49): return ('event', 49)
    return None

CASTLE_NAME = (24, 25, 13)     # flag, name offset, size in a 70-byte castle block
HERO_NAME = (45, 46, 13)       # in a 76-byte hero block

def texts(path):
    """[(key, kind, english, limit_bytes or 0)] ; key = 'H:name' | 'H:desc' | 'B:<i>:<off>' | 'F:<i>:<off>' (fixed field)"""
    m = mp2.Map(path); out = []
    if m.name(): out.append(('H:name', 'name', m.name(), mp2.NAME_LEN - 1))
    if m.desc(): out.append(('H:desc', 'desc', m.desc(), mp2.DESC_LEN - 1))
    for i, b in enumerate(m.blocks):
        if len(b) == 70 and b[CASTLE_NAME[0]] == 1:
            t = bytes(b[CASTLE_NAME[1]:CASTLE_NAME[1] + CASTLE_NAME[2]]).split(b'\0')[0]
            if printable(t): out.append(('F:%d:%d' % (i, CASTLE_NAME[1]), 'town name', t.decode('latin-1'), CASTLE_NAME[2] - 1))
            continue
        if len(b) == 76 and b[HERO_NAME[0]] == 1:
            t = bytes(b[HERO_NAME[1]:HERO_NAME[1] + HERO_NAME[2]]).split(b'\0')[0]
            if printable(t): out.append(('F:%d:%d' % (i, HERO_NAME[1]), 'hero name', t.decode('latin-1'), HERO_NAME[2] - 1))
            continue
        k = block_kind(b)
        if not k or k[0] == 'sphinx': continue        # sphinx riddles: the answer is typed in English, keep them
        t = bytes(b[k[1]:-1]).decode('latin-1')
        if t.strip(): out.append(('B:%d:%d' % (i, k[1]), k[0], t, 0))
    return out

def map_files():
    return sorted(glob.glob(os.path.join(MAPS, '*')), key=lambda p: os.path.basename(p).upper())

if __name__ == '__main__':
    items = []; n = 0
    for f in map_files():
        bn = os.path.basename(f)
        try: tt = texts(f)
        except Exception as e: print('skip', bn, e); continue
        for key, kind, en, lim in tt:
            items.append({'id': 'M:%s:%s' % (bn.upper(), key), 'en': en, 'map': bn, 'kind': kind, 'limit': lim})
            n += len(en)
    json.dump(items, open(os.path.join(ROOT, 'text', 'src', 'maps.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    u = len(set(i['en'] for i in items))
    print(len(items), 'texts', u, 'unique', n, 'chars', sum(len(e) for e in set(i['en'] for i in items)), 'unique chars')
