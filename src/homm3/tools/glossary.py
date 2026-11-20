# build text/glossary.md (names) from text/out/names_*.json
#  - same English in several places -> one Japanese (category priority below), written to text/fixes_auto.json
#  - text/fixes.json (hand fixes) always wins
import json, os, glob, collections
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
PRIO = ['creatures', 'spells', 'artifacts', 'heroes', 'towns', 'skills', 'buildings', 'objects', 'campaign', 'hota']
# names are unified only inside a group (Inferno the spell != Inferno the town)
GROUP = {'buildings': 'b', 'objects': 'b', 'artifacts': 'a', 'heroes': 'a', 'campaign': 'a'}
gk = lambda cat, en: (GROUP.get(cat, cat), en.lower())

def norm(en, ja):
    ja = ja.strip()
    if ja.startswith('「') and ja.endswith('」') and '"' not in en: ja = ja[1:-1]
    return ja

fixes = json.load(open(T('fixes.json'), encoding='utf-8')) if os.path.exists(T('fixes.json')) else {}
entries = []   # (cat, id, en, raw ja)
for p in glob.glob(T('chunks', 'names_*.json')) + glob.glob(T('chunks', 'hnames_*.json')):
    name = os.path.basename(p)[:-5]; op = T('out', name + '.json')
    if not os.path.exists(op): continue
    cat = 'hota' if name.startswith('hnames') else name.split('_')[1]
    out = json.load(open(op, encoding='utf-8'))
    for it in json.load(open(p, encoding='utf-8'))['items']:
        if it['id'] in out: entries.append((cat, it['id'], it['en'].strip(), out[it['id']]))
entries.sort(key=lambda e: (PRIO.index(e[0]), e[1]))
best = {}
for cat, i, en, raw in entries:            # hand fixes first
    if i in fixes: best[gk(cat, en)] = fixes[i]
for cat, i, en, raw in entries:
    best.setdefault(gk(cat, en), norm(en, raw))
auto = {}
for cat, i, en, raw in entries:
    if i in fixes: continue
    if raw != best[gk(cat, en)]: auto[i] = best[gk(cat, en)]
json.dump(auto, open(T('fixes_auto.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
cats = collections.OrderedDict((c, collections.OrderedDict()) for c in PRIO)
for cat, i, en, raw in entries:
    if en.upper().startswith('NOT USED') or best[gk(cat, en)] == en: continue
    cats[cat].setdefault(en, best[gk(cat, en)])
with open(T('glossary.md'), 'w', encoding='utf-8') as f:
    f.write('# HoMM3 用語集（固有名）\n\n自動生成（tools/glossary.py）。修正は text/fixes.json で。\n')
    for cat, d in cats.items():
        if not d: continue
        f.write(f'\n## {cat}\n| English | 日本語 |\n|---|---|\n')
        for en, ja in d.items(): f.write(f'| {en} | {ja} |\n')
print({c: len(d) for c, d in cats.items()}, 'auto-unified', len(auto))
