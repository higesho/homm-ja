# text/out/*.json + fixes_auto.json + fixes.json -> text/ja/txt.json (TXT cells) and text/ja/map.json (map strings)
import json, os, glob, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
def load(p): return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
items = {}
for fn in ('hota_txt_units.json', 'hota_map_units.json'):
    if os.path.exists(T('src', fn)):
        for it in json.load(open(T('src', fn), encoding='utf-8')): items[it['id']] = it['en']
for p in glob.glob(T('chunks', '*.json')):
    for it in json.load(open(p, encoding='utf-8'))['items']: items[it['id']] = it['en']
out = {}
if os.path.exists(T('src', 'hota_txt_units.json')):
    for it in json.load(open(T('src', 'hota_txt_units.json'), encoding='utf-8')):
        if 'ja' in it: out[it['id']] = it['ja']
for p in sorted(glob.glob(T('out', '*.json'))):
    try: out.update(load(p))
    except Exception as e: print('bad', p, e)
out.update(load(T('fixes_auto.json'))); out.update(load(T('fixes.json')))
def clean(en, ja):
    ja = ja.replace('\r\n', '\n')
    if ja.startswith('「') and ja.endswith('」') and not en.startswith('"') and ja.count('「') == 1: ja = ja[1:-1]
    # keep the English leading/trailing whitespace
    if v == '': return ''
    lead = re.match(r'^\s*', en).group(); trail = re.search(r'\s*$', en).group()
    return lead + ja.strip() + trail if ja.strip() else ja
REP = load(T('replace.json'))
txt, mp, hx = {}, {}, {}
for k, v in out.items():
    if k not in items or not isinstance(v, str): continue
    if v == '' and (k.startswith('M:') or not items[k].strip() == items[k].strip()): continue
    en = items[k]
    if v == en: continue                      # left in English on purpose
    v = clean(en, v)
    for a, b in REP.items(): v = v.replace(a, b)
    (mp if k.startswith('M:') else hx if k.startswith('H:') else txt)[k] = v
os.makedirs(T('ja'), exist_ok=True)
json.dump(txt, open(T('ja', 'txt.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
json.dump(mp, open(T('ja', 'map.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
json.dump(hx, open(T('ja', 'hota_txt.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print('txt', len(txt), 'map', len(mp), 'hota txt', len(hx))
