# make translation chunks: text/chunks/<name>.json  ->  translators write text/out/<name>.json ({id: ja})
#   python tools/chunk.py names      name lists for the glossary
#   python tools/chunk.py content    everything else (needs text/glossary.md from the names pass)
import sys, os, json, re, collections
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = lambda *p: os.path.join(ROOT, 'text', *p)

def cp(s):  # show cp1252 punctuation readably
    return s.encode('latin-1').decode('cp1252', errors='replace')

NAME_SETS = [
    ('creatures', [('crtraits.txt', '0'), ('crtraits.txt', '1')]),
    ('artifacts', [('artraits.txt', '0')]),
    ('spells', [('sptraits.txt', '0'), ('sptraits.txt', '1')]),
    ('skills', [('sstraits.txt', '0'), ('herospec.txt', '0'), ('herospec.txt', '1'), ('hctraits.txt', '0'),
                ('priskill.txt', '0'), ('skilllev.txt', '0'), ('castinfo.txt', '0')]),
    ('heroes', [('hotraits.txt', '0')]),
    ('towns', [('townname.txt', '0'), ('towntype.txt', '0')]),
    ('objects', [('objnames.txt', '0'), ('minename.txt', '0'), ('crgen1.txt', '0'), ('crgen4.txt', '0'), ('crbanks.txt', '0'),
                 ('garrison.txt', '0'), ('crgenerc.txt', '0'), ('monolith.txt', '0'), ('plcolors.txt', '0'),
                 ('tentcolr.txt', '0'), ('terrname.txt', '0'), ('restypes.txt', '0'), ('artslots.txt', '0')]),
    ('buildings', [('bldgneut.txt', '0'), ('bldgspec.txt', '0'), ('dwelling.txt', '0')]),
    ('campaign', [('camptext.txt', '0'), ('camphigh.txt', '1'), ('camphigh.txt', '2')]),
]
NAME_KEYS = {k for _, ks in NAME_SETS for k in ks}

def load_units():
    tu = json.load(open(T('src', 'txt_units.json'), encoding='utf-8'))
    mu = json.load(open(T('src', 'map_units.json'), encoding='utf-8'))
    return tu, mu

def key(u):
    f, r, c = u['id'].rsplit(':', 2); return (f, c)

def write_chunk(name, kind, items, extra=None):
    d = {'name': name, 'kind': kind, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(T('chunks', name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def names():
    tu, mu = load_units()
    by = collections.defaultdict(list)
    for u in tu:
        if key(u) in NAME_KEYS: by[key(u)].append(u)
    for cat, keys in NAME_SETS:
        items = []
        for k in keys:
            for u in by[k]:
                items.append({'id': u['id'], 'en': cp(u['en']), 'ctx': f'{k[0]} col{k[1]}'})
        items.sort(key=lambda it: (it['id'].rsplit(':', 2)[0], int(it['id'].rsplit(':', 2)[1]), int(it['id'].rsplit(':', 2)[2])))
        n = (len(items) + 179) // 180
        per = (len(items) + n - 1) // n
        for i in range(n):
            write_chunk(f'names_{cat}_{i+1}', 'names', items[i * per:(i + 1) * per])
            print(f'names_{cat}_{i+1}', len(items[i * per:(i + 1) * per]))

def glossary_terms():
    """English -> Japanese from glossary.md tables (core + names)"""
    g = {}
    for fn in ('glossary_core.md', 'glossary.md'):
        p = T(fn)
        if not os.path.exists(p): continue
        for line in open(p, encoding='utf-8'):
            m = re.match(r'\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|', line)
            if not m or m.group(1) in ('English', '---'): continue
            g[m.group(1)] = m.group(2)
    return g

def sub_glossary(text, g):
    out = {}
    low = text.lower()
    for en, ja in g.items():
        for part in en.split(' / '):
            p = re.sub(r'\s*\(.*?\)', '', part).strip().lower()
            if len(p) >= 3 and re.search(r'(?<![a-z])' + re.escape(p) + r'(?![a-z])', low):
                out[en] = ja; break
    return out

def content(limit=9000):
    tu, mu = load_units()
    g = glossary_terms()
    names_done = {}
    for fn in os.listdir(T('out')) if os.path.isdir(T('out')) else []:
        if fn.startswith('names_'): names_done.update(json.load(open(T('out', fn), encoding='utf-8')))
    # TXT content grouped by file
    groups = collections.OrderedDict()
    for u in tu:
        if key(u) in NAME_KEYS: continue
        groups.setdefault(key(u)[0], []).append({'id': u['id'], 'en': cp(u['en'])})
    chunks = []
    cur, size, n = [], 0, 1
    for f, items in groups.items():
        for it in items:
            if cur and size + len(it['en']) > limit:
                chunks.append((f'txt_{n:03d}', cur)); cur, size, n = [], 0, n + 1
            it['file'] = f
            cur.append(it); size += len(it['en'])
    if cur: chunks.append((f'txt_{n:03d}', cur))
    # map strings grouped by first source map
    mg = collections.OrderedDict()
    for u in mu:
        src = u['src'][0].split('#')[0]
        mg.setdefault(src, []).append({'id': u['id'], 'en': cp(u['en']), 'map': src})
    cur, size, n = [], 0, 1
    camp, cs, cn = [], 0, 1
    for src, items in mg.items():
        is_c = src.lower().endswith('.h3c')
        for it in items:
            if is_c:
                if camp and cs + len(it['en']) > limit and camp[-1]['map'] != src or (camp and cs + len(it['en']) > limit * 1.4):
                    chunks.append((f'camp_{cn:03d}', camp)); camp, cs, cn = [], 0, cn + 1
                camp.append(it); cs += len(it['en'])
            else:
                if cur and size + len(it['en']) > limit:
                    chunks.append((f'map_{n:03d}', cur)); cur, size, n = [], 0, n + 1
                cur.append(it); size += len(it['en'])
    if cur: chunks.append((f'map_{n:03d}', cur))
    if camp: chunks.append((f'camp_{cn:03d}', camp))
    for name, items in chunks:
        text = ' '.join(i['en'] for i in items)
        write_chunk(name, 'content', items, {'glossary': sub_glossary(text, g)})
    print(len(chunks), 'content chunks')

if __name__ == '__main__':
    os.makedirs(T('chunks'), exist_ok=True); os.makedirs(T('out'), exist_ok=True)
    {'names': names, 'content': content}[sys.argv[1]]()
