# HotA TXT (dumped from the game into ext/hota_txt) -> text/src/hota_txt_units.json
#   each unit: id "H:file:row:col", en, ja (reused from the Complete translation if the same English exists)
import sys, os, json, re, glob, collections
sys.path.insert(0, os.path.dirname(__file__))
from txt import parse
import extract as X
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
SKIP = X.SKIPF | {'helptxt', 'default'}
def units():
    out = []
    for p in sorted(glob.glob(os.path.join(ROOT, 'ext', 'hota_txt', '*'))):
        low = os.path.basename(p).lower()
        stem, ext = os.path.splitext(low)
        if ext != '.txt' or stem in SKIP: continue
        cols, hdr = X.RULES.get(stem, (None, 0))
        rows = parse(open(p, 'rb').read())
        for ri, (row, eol) in enumerate(rows):
            if ri < hdr: continue
            for ci, (t, q, tail) in enumerate(row):
                if cols is not None and ci not in cols: continue
                if X.keep_en(t): continue
                if stem == 'help' and t.strip() == row[0][0].strip(): continue
                out.append({'id': f'H:{low}:{ri}:{ci}', 'en': t})
            if stem == 'help' and ri >= 1 and len(row) >= 2:
                c0, c1 = row[0][0], row[1][0]
                if c1.strip() and c0.strip() != c1.strip() and not X.keep_en(c0) and not re.fullmatch(r'[A-Z0-9_ ]+', c0.strip()):
                    out.append({'id': f'H:{low}:{ri}:0', 'en': c0})
    return out
if __name__ == '__main__':
    tu = {u['id']: u['en'] for u in json.load(open(T('src', 'txt_units.json'), encoding='utf-8'))}
    ja = json.load(open(T('ja', 'txt.json'), encoding='utf-8'))
    byen = collections.defaultdict(collections.Counter)
    for k, en in tu.items():
        if k in ja: byen[en][ja[k]] += 1
    us = units(); new = 0; newc = 0
    for u in us:
        f, r, c = u['id'][2:].rsplit(':', 2)
        k = f'{f}:{r}:{c}'
        if k in tu and tu[k] == u['en'] and k in ja: u['ja'] = ja[k]
        elif u['en'] in byen: u['ja'] = byen[u['en']].most_common(1)[0][0]
        else: new += 1; newc += len(u['en'])
    json.dump(us, open(T('src', 'hota_txt_units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print('units', len(us), 'reused', len(us) - new, 'new', new, 'chars', newc)
