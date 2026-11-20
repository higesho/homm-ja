# HotA translation chunks: python tools/hota_chunks.py names | content
import sys, os, json, re, collections
sys.path.insert(0, os.path.dirname(__file__))
from chunk import write_chunk, glossary_terms, sub_glossary, cp
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)
NAMECOLS = {('crtraits.txt', '0'), ('crtraits.txt', '1'), ('artraits.txt', '0'), ('hctraits.txt', '0'), ('hotraits.txt', '0'),
            ('crgen1.txt', '0'), ('bldgspec.txt', '0'), ('bldgneut.txt', '0'), ('dwelling.txt', '0'), ('objnames.txt', '0'),
            ('terrname.txt', '0'), ('sptraits.txt', '0'), ('sptraits.txt', '1'), ('herospec.txt', '0'), ('herospec.txt', '1'),
            ('townname.txt', '0'), ('crbanks.txt', '0'), ('minename.txt', '0'), ('camptext.txt', '0')}
def load():
    tu = [u for u in json.load(open(T('src', 'hota_txt_units.json'), encoding='utf-8')) if 'ja' not in u]
    mu = [u for u in json.load(open(T('src', 'hota_map_units.json'), encoding='utf-8')) if not u['old']]
    return tu, mu
def is_name(s): return len(s) <= 32 and not re.search(r'[.!?\n%{]', s)
def names():
    tu, mu = load(); items = []
    for u in tu:
        f, r, c = u['id'][2:].rsplit(':', 2)
        if (f, c) in NAMECOLS and is_name(u['en']): items.append({'id': u['id'], 'en': cp(u['en']), 'ctx': f'{f} col{c}'})
    for u in mu:
        if 'HotA.dat' in u['src']:
            if is_name(u['en']): items.append({'id': u['id'], 'en': cp(u['en']), 'ctx': 'HotA.dat (data of new creatures, heroes, artifacts, spells, objects, towns, in file order)'})
    n = (len(items) + 199) // 200
    for i in range(n):
        part = items[i * len(items) // n:(i + 1) * len(items) // n]
        write_chunk(f'hnames_{i+1:02d}', 'names', part); print(f'hnames_{i+1:02d}', len(part))
def content(limit=9000):
    tu, mu = load(); g = glossary_terms()
    done = set()
    for fn in os.listdir(T('chunks')):
        if fn.startswith('hnames_'):
            for it in json.load(open(T('chunks', fn), encoding='utf-8'))['items']: done.add(it['id'])
    chunks = []
    def pack(prefix, items):
        cur, size, n = [], 0, 1
        for it in items:
            if cur and size + len(it['en']) > limit:
                chunks.append((f'{prefix}_{n:03d}', cur)); cur, size, n = [], 0, n + 1
            cur.append(it); size += len(it['en'])
        if cur: chunks.append((f'{prefix}_{n:03d}', cur))
    pack('htxt', [{'id': u['id'], 'en': cp(u['en']), 'file': u['id'][2:].rsplit(':', 2)[0]} for u in tu if u['id'] not in done])
    pack('hdat', [{'id': u['id'], 'en': cp(u['en']), 'map': 'HotA.dat (texts of new HotA creatures, heroes, artifacts, spells, objects, towns)'}
                  for u in mu if 'HotA.dat' in u['src'] and u['id'] not in done])
    pack('hcamp', [{'id': u['id'], 'en': cp(u['en']), 'map': u['src'][0].split('#')[0]} for u in mu if '.h3c' in u['src'][0].lower() and 'HotA.dat' not in u['src'] and u['id'] not in done])
    pack('hmap', [{'id': u['id'], 'en': cp(u['en']), 'map': u['src'][0].split('#')[0]} for u in mu
                  if 'HotA.dat' not in u['src'] and '.h3c' not in u['src'][0].lower() and u['id'] not in done])
    for name, items in chunks:
        write_chunk(name, 'content', items, {'glossary': sub_glossary(' '.join(i['en'] for i in items), g)})
    print(len(chunks), 'content chunks', collections.Counter(n.split('_')[0] for n, _ in chunks))
def lists(limit=7000):
    # HotA.dat numbered text lists (UI strings, rumors, rune texts) not yet translated
    mj = json.load(open(T('ja', 'map.json'), encoding='utf-8')); g = glossary_terms()
    mu = json.load(open(T('src', 'hota_map_units.json'), encoding='utf-8'))
    items = [{'id': u['id'], 'en': cp(u['en']), 'map': 'HotA.dat text list (HotA user interface strings, messages, rumors)'}
             for u in mu if 'HotA.dat#list' in u['src'] and not mj.get(u['id'])
             and not (' ' not in u['en'] and re.search(r'.[A-Z0-9_]', u['en']))]
    cur, size = [], 0
    n = 1 + len([f for f in os.listdir(T('chunks')) if f.startswith('hlst_')])
    for it in items + [None]:
        if it is None or (cur and size + len(it['en']) > limit):
            name = f'hlst_{n:03d}'; write_chunk(name, 'content', cur, {'glossary': sub_glossary(' '.join(i['en'] for i in cur), g)})
            print(name, len(cur)); cur, size, n = [], 0, n + 1
            if it is None: break
        cur.append(it); size += len(it['en'])
if __name__ == '__main__':
    {'names': names, 'content': content, 'lists': lists}[sys.argv[1]]()
