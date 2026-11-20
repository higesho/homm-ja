# validate translator output: python tools/check_chunk.py [chunk names...]  (default: all with output)
import sys, os, json, re, glob
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
FMT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?[sdcuxXif%]')

def euc_ok(s):
    bad = []
    for c in s:
        try: c.encode('euc_jp')
        except UnicodeEncodeError:
            if c not in '～‐—−･｡ “”‘’…': bad.append(c)
    return bad

def check(name):
    items = json.load(open(T('chunks', name + '.json'), encoding='utf-8'))['items']
    op = T('out', name + '.json')
    if not os.path.exists(op): return [f'{name}: no output']
    try: out = json.load(open(op, encoding='utf-8'))
    except Exception as e: return [f'{name}: bad json {e}']
    errs = []
    for it in items:
        i, en = it['id'], it['en']
        ja = out.get(i)
        if ja is None: errs.append(f'{i}: missing'); continue
        if not isinstance(ja, str): errs.append(f'{i}: not a string'); continue
        if sorted(FMT.findall(en)) != sorted(FMT.findall(ja)): errs.append(f'{i}: format {FMT.findall(en)} -> {FMT.findall(ja)}')
        elif FMT.findall(en) != FMT.findall(ja): errs.append(f'{i}: format order {FMT.findall(en)} -> {FMT.findall(ja)}')
        if en.count('{') != ja.count('{') or en.count('}') != ja.count('}'): errs.append(f'{i}: braces')
        b = euc_ok(ja)
        if b: errs.append(f'{i}: non-EUC chars {b}')
        if len(en) > 25 and re.search(r'[A-Za-z]{4,}(?: [A-Za-z]{3,}){3}', ja): errs.append(f'{i}: English left? {ja[:40]!r}')
        if len(re.sub(r'[\s]', '', en)) > 3 and not ja.strip(): errs.append(f'{i}: empty')
    extra = set(out) - {it['id'] for it in items}
    if extra: errs.append(f'{name}: {len(extra)} unknown ids')
    return errs

if __name__ == '__main__':
    names = sys.argv[1:] or [os.path.basename(p)[:-5] for p in sorted(glob.glob(T('out', '*.json')))]
    tot = 0
    for n in names:
        e = check(n); tot += len(e)
        for x in e[:15]: print(n, x)
        if len(e) > 15: print(n, '...', len(e) - 15, 'more')
    print('errors', tot)
