# text/out/*.json (id -> ja) -> text/ja.json (English -> Japanese) and text/glossary.json (names, for the content chunks)
#   priority: text/fixes.json (English-keyed) > out/ ; text/replace.json unifies Japanese spellings
import os, sys, json, glob, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def fix_ws(en, t):
    """keep the English leading/trailing whitespace"""
    lead = en[:len(en) - len(en.lstrip())]; trail = en[len(en.rstrip()):]
    return lead + t.strip() + trail

def main():
    ja = {}; G = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', '*.json'))):
        op = os.path.join(ROOT, 'text', 'out', os.path.basename(p))
        if not os.path.exists(op): continue
        try: out = json.load(open(op, encoding='utf-8'))
        except Exception as e: print('bad', op, e); continue
        isname = os.path.basename(p).startswith('names_')
        for i in json.load(open(p, encoding='utf-8'))['items']:
            t = out.get(i['id'])
            if not (isinstance(t, str) and t): continue
            if 'src' in i: ja[i['en']] = fix_ws(i['en'], t)
            if isname and t != i['en'] and len(i['en']) > 1:
                if 'src' not in i and ' ' not in i['en'] and not re.fullmatch(r'[゠-ヿ・＝]+', t): continue   # ordinary words
                G[i['en'].strip()] = t.strip()
    json.dump(G, open(os.path.join(ROOT, 'text', 'glossary.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    rp = os.path.join(ROOT, 'text', 'replace.json')
    if os.path.exists(rp):
        R = json.load(open(rp, encoding='utf-8'))
        pats = [(re.compile(r'(?<![ァ-ー])' + re.escape(a) + r'(?![ァ-ー])'), b) for a, b in R.items()]
        for k in ja:
            for p, b in pats: ja[k] = p.sub(b, ja[k])
    fx = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(fx): ja.update(json.load(open(fx, encoding='utf-8')))
    json.dump(ja, open(os.path.join(ROOT, 'text', 'ja.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    items = json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))
    print(len(ja), 'of', len(items), 'translated;', len(G), 'glossary names')

if __name__ == '__main__':
    main()
