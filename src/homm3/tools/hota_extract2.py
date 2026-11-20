# HotA maps / campaigns / HotA.dat strings -> text/src/hota_map_units.json (id "M:<sha12>", same scheme as the Complete map units)
import sys, os, json, re, glob, hashlib, struct
sys.path.insert(0, os.path.dirname(__file__))
from mapstr import gunzip_all, strings
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
HOTA = r'L:/Games/GOG/TocaRaceDriver3/Heroes of Might and Magic III - Horn of the Abyss'
def dat_strings(d):
    """same scan as the DLL: u32 length + printable text containing a letter"""
    i = 12; out = []
    while i + 4 <= len(d):
        L = struct.unpack_from('<I', d, i)[0]
        if 1 <= L < 400000 and i + 4 + L <= len(d):
            b = d[i + 4:i + 4 + L]
            if all(32 <= c < 127 or c in (9, 10, 13) or c >= 0x80 for c in b) and re.search(rb'[A-Za-z]', b):
                out.append(b.decode('latin-1')); i += 4 + L; continue
        i += 1
    return out
DQ = chr(34)
def dat_list(s):
    """numbered text lists in HotA.dat ('# 0' / '# Names' / '# Texts' / '... rumors'); mirrored in text.c dat_list"""
    f = s.split(chr(13) + chr(10), 1)[0]
    return bool(re.fullmatch(r'# \d+ ?', f) or f in ('# Names', '# Texts') or f.lower().endswith('rumors'))
def dat_parse(s):
    """-> [('raw', text) | ('val', text, quoted)]; a value is a non-comment line with a letter, or a quoted block"""
    out = []; i = 0; n = len(s); NL = chr(13) + chr(10)
    while i < n:
        if s[i] == DQ:
            j = i + 1; buf = ''
            while j < n:
                if s[j] == DQ:
                    if j + 1 < n and s[j + 1] == DQ: buf += DQ; j += 2; continue
                    break
                buf += s[j]; j += 1
            out.append(('val', buf, True)); i = j + 1
            e = s.find(NL, i)
            if e < 0: e = n
            if s[i:e]: out.append(('raw', s[i:e]))
            if e < n: out.append(('raw', NL))
            i = e + 2 if e < n else n
        else:
            e = s.find(NL, i)
            if e < 0: e = n
            line = s[i:e]
            if line.startswith('#') or not re.search('[A-Za-z]', line): out.append(('raw', line))
            else: out.append(('val', line, False))
            if e < n: out.append(('raw', NL))
            i = e + 2 if e < n else n
    return out
def dat_text(s):
    # mirrored in the DLL (text.c dat_text)
    if re.search(r'\.(def|str|pcx|wav|smk|bik|msk|txt|h3m|fnt)$', s, re.I): return False
    if re.fullmatch(r'[a-z0-9_]{1,10}|[A-Z0-9_]{1,10}', s): return False
    if s.startswith('#'): return False
    if '	' in s or not re.search(r'[A-Z]', s): return False       # embedded tables / lowercase key lists
    if ' ' not in s and re.search(r'.[A-Z0-9_]', s): return False   # identifiers: CoveTown, Rune1, rmg_x
    return len(s) >= 3
if __name__ == '__main__':
    old = {u['id'] for u in json.load(open(T('src', 'map_units.json'), encoding='utf-8'))}
    out = {}; order = []
    def add(s, src):
        h = hashlib.sha1(s.encode('latin-1')).hexdigest()[:12]
        if h not in out: out[h] = {'id': 'M:' + h, 'en': s, 'src': [], 'old': ('M:' + h) in old}; order.append(h)
        if src not in out[h]['src']: out[h]['src'].append(src)
    for p in sorted(glob.glob(os.path.join(HOTA, 'Maps', '*'))):
        for k, d in enumerate(gunzip_all(open(p, 'rb').read())):
            for off, s in strings(d):
                if re.search(r'\.(def|msk|h3m)$', s, re.I) or len(s.strip()) < 2: continue
                add(s, f'{os.path.basename(p)}#{k}')
    for p in sorted(glob.glob(os.path.join(ROOT, 'ext', 'hota_lng', '*.h3c'))):
        for k, d in enumerate(gunzip_all(open(p, 'rb').read())):
            for off, s in strings(d):
                if re.search(r'\.(def|msk|h3m)$', s, re.I) or len(s.strip()) < 2: continue
                add(s, f'{os.path.basename(p)}#{k}')
    for s in dat_strings(open(os.path.join(HOTA, 'HotA.dat'), 'rb').read()):
        if dat_text(s): add(s, 'HotA.dat')
        elif s.startswith('#') and dat_list(s):
            for t in dat_parse(s):
                if t[0] == 'val': add(t[1], 'HotA.dat#list')
    us = [out[h] for h in order]
    json.dump(us, open(T('src', 'hota_map_units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    new = [u for u in us if not u['old']]
    print('units', len(us), 'reused', len(us) - len(new), 'new', len(new), 'chars', sum(len(u['en']) for u in new))
    import collections
    c = collections.Counter(); cc = collections.Counter()
    for u in new:
        k = 'dat' if u['src'][0] == 'HotA.dat' else ('camp' if '.h3c' in u['src'][0] else 'map')
        c[k] += 1; cc[k] += len(u['en'])
    print(c, cc)
