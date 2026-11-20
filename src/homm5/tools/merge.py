# text/out/*.json (id -> ja) -> text/ja.json (English -> Japanese) and text/glossary.json (names, for the content chunks)
#   priority: text/fixes.json (English-keyed) > out/
#   "the bandit" / "a Vial of ..." (creatures:2/4, Artifacts:3) = the Japanese of the name in the same row
import os, sys, json, glob, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def load_out():
    ids = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'out', '*.json'))):
        try: ids.update(json.load(open(p, encoding='utf-8')))
        except Exception as e: print('bad', p, e)
    return ids

CODE = re.compile(r'<value=[A-Za-z_0-9]+>')
def fix_ws(en, t):
    """keep the English leading/trailing whitespace and newlines, and the case of the % codes"""
    lead = en[:len(en) - len(en.lstrip())]; trail = en[len(en.rstrip()):]
    t = lead + t.strip() + trail
    want = {c.lower(): c for c in CODE.findall(en)}
    return CODE.sub(lambda m: want.get(m.group().lower(), m.group()), t)

def main():
    items = json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))
    ja = {}
    # ids are per chunk file: map through each chunk's own (id, en) list
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))):
        op = os.path.join(ROOT, 'text', 'out', os.path.basename(p))
        if not os.path.exists(op): continue
        try: out = json.load(open(op, encoding='utf-8'))
        except Exception as e: print('bad', op, e); continue
        for i in json.load(open(p, encoding='utf-8'))['items']:
            t = out.get(i['id'])
            if isinstance(t, str) and t: ja[i['en']] = fix_ws(i['en'], t)
    ids = load_out()
    # glossary from the names chunks (+ proper nouns)
    G = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'names_*.json'))):
        op = os.path.join(ROOT, 'text', 'out', os.path.basename(p))
        out = json.load(open(op, encoding='utf-8')) if os.path.exists(op) else {}
        for i in json.load(open(p, encoding='utf-8'))['items']:
            t = out.get(i['id'])
            if not (isinstance(t, str) and t and t != i['en'] and len(i['en']) > 1): continue
            if 'src' not in i and ' ' not in i['en'] and not re.fullmatch(r'[゠-ヿ・]+', t): continue   # ordinary words
            G[i['en']] = t
    json.dump(G, open(os.path.join(ROOT, 'text', 'glossary.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    rp = os.path.join(ROOT, 'text', 'replace.json')   # Japanese spelling variants -> one spelling
    if os.path.exists(rp):
        R = json.load(open(rp, encoding='utf-8'))
        pats = [(re.compile(r'(?<![ァ-ー])' + re.escape(a) + r'(?![ァ-ー])'), b) for a, b in R.items()]
        for k in ja:
            for p, b in pats: ja[k] = p.sub(b, ja[k])
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx): ja.update(json.load(open(fx, encoding='utf-8')))
    json.dump(ja, open(os.path.join(ROOT, 'text', 'ja.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(ja), 'of', len(items), 'translated;', len(G), 'glossary names')

if __name__ == '__main__':
    main()
