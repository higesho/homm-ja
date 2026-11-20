# translation chunks -> text/chunks/*.json
#   python tools/chunk.py names    : names_* chunks (creature/artifact/spell/skill/hero/building names + proper nouns of the maps)
#   python tools/chunk.py content  : the rest, with a per-chunk glossary from text/glossary.json
import os, sys, json, re, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CH = os.path.join(ROOT, 'text', 'chunks')
NAME_SRC = {
    'A': ['creatures:1', 'creatures:3', 'creature_abilities:1', 'buildings:1', 'Spells:1', 'Spells:2', 'creatures:26'],
    'B': ['Artifacts:2', 'skills:1'],
    'C': ['heroes:1', 'random_names:1'],
}
DERIVED = ['creatures:2', 'creatures:4', 'Artifacts:3']    # "the bandit", "a Vial of..." = same as the name
COMMON = set('''The You Your This That If When It As With After But And In On At For To Of We He She They His Her Their There What Who
Why How Now Then So Once Only Do Does Did Are Is Was Were Be Will Would Can Could Should May Might Must Not No Yes Oh Well Good Thank
Please Here All Any Some Each Every One Two Three Four Five Six Seven Eight Nine Ten My Perhaps Even Soon Bring Unfortunately Give Lose
Just Defeat Finally Don Let An By These Take Meanwhile Today Maybe Find Or Tell Come Still Sorry From Day Our While Ah Everyone Instead
Have Though Where More Something Uh Suddenly Having Yeah First Go Because Those Apparently Immediately Someone Later Before Although
Again Also Many Most Much Other Over Since Such Than Very Yet Within Without Already Whatever However Unless Until Upon Who Whom Why
Click Select Show Hide Use Return Cancel Okay Ok Note Hint Warning Victory Defeat Congratulations Welcome Great Hello Hey Ha Hmm Wow
Look Remember Beware Kill Capture Build Hire Recruit Visit Win Hold Keep Get Gather Collect Make Move Send Help Leave Stop Wait'''.split())

def load():
    return json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))

def write(name, items, extra=None):
    os.makedirs(CH, exist_ok=True)
    d = {'name': name, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(os.path.join(CH, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def names():
    it = load()
    have = set()
    for k, srcs in NAME_SRC.items():
        items = [{'id': i['id'], 'en': i['en'], 'kind': i['src']} for i in it if i['src'] in srcs]
        have |= {i['en'] for i in items}
        write('names_' + k, items)
        print('names_' + k, len(items))
    # proper nouns of the map / UI text: capitalised phrases seen 3+ times
    C = collections.Counter()
    for i in it:
        if i['src'].startswith('map') or i['src'].startswith('heroes:4') or i['src'].startswith('Artifacts:9'):
            for m in re.finditer(r"\b([A-Z][a-z'\-]+(?: (?:of the |of |the |von |Von )?[A-Z][a-z'\-]+){0,3})\b", i['en']):
                w = m.group(1)
                if w.split()[0] in COMMON or re.search(r"'(s|m|re|ll|ve|d|t)$", w) or w.endswith('-'): continue
                C[w] += 1
    pn = [w for w, c in C.most_common() if c >= 3 and w not in have]
    items = [{'id': 'pn%04d' % k, 'en': w} for k, w in enumerate(pn)]
    for k in range(0, len(items), 400):
        write('names_D%d' % (k // 400), items[k:k + 400])
        print('names_D%d' % (k // 400), len(items[k:k + 400]))

def gloss_for(text, G, keys):
    out = {}
    low = text.lower()
    for k in keys:
        if k.lower() in low and re.search(r'(?<![A-Za-z])' + re.escape(k.lower()) + r'(?![a-z])', low): out[k] = G[k]
    return out

def content(limit=22000):
    it = load()
    G = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    keys = sorted(G, key=len, reverse=True)
    skip = set()
    for srcs in NAME_SRC.values(): skip |= set(srcs)
    skip |= set(DERIVED)
    groups = collections.OrderedDict()
    for i in it:
        if i['src'] in skip: continue
        g = i['src'].split(':')[0] if not i['src'].startswith('map:') else i['src']
        groups.setdefault(g, []).append(i)
    chunks = []; cur = []; size = 0; curg = None
    def flush():
        nonlocal cur, size
        if cur: chunks.append((curg, cur))
        cur = []; size = 0
    for g, items in groups.items():
        ismap = g.startswith('map:')
        if not ismap or (curg and not curg.startswith('map')): flush()
        curg = 'map' if ismap else g
        for i in items:
            if size + len(i['en']) > limit and cur: flush(); curg = 'map' if ismap else g
            cur.append({'id': i['id'], 'en': i['en'], 'src': i['src']}); size += len(i['en'])
        if not ismap: flush()
    flush()
    n = collections.Counter()
    for g, items in chunks:
        tag = re.sub(r'[^A-Za-z0-9]+', '_', g).strip('_').lower()
        n[tag] += 1
        name = '%s_%02d' % (tag, n[tag])
        text = '\n'.join(i['en'] for i in items)
        write(name, items, {'glossary': gloss_for(text, G, keys)})
    print(len(chunks), 'chunks', dict(n))

if __name__ == '__main__':
    {'names': names, 'content': content}[sys.argv[1]]()
