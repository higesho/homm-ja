# translation chunks -> text/chunks/*.json
#   python tools/chunk.py names    : names_* (kind=name strings + capitalised proper nouns of the story texts)
#   python tools/chunk.py content  : the rest (kind=text), grouped by folder, with a per-chunk glossary (text/glossary.json)
import os, sys, re, json, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CH = os.path.join(ROOT, 'text', 'chunks')
COMMON = set('''The You Your This That If When It As With After But And In On At For To Of We He She They His Her Their There What Who
Why How Now Then So Once Only Do Does Did Are Is Was Were Be Will Would Can Could Should May Might Must Not No Yes Oh Well Good Thank
Please Here All Any Some Each Every One Two Three Four Five Six Seven Eight Nine Ten My Perhaps Even Soon Bring Unfortunately Give Lose
Just Defeat Finally Don Let An By These Take Meanwhile Today Maybe Find Or Tell Come Still Sorry From Day Our While Ah Everyone Instead
Have Though Where More Something Uh Suddenly Having Yeah First Go Because Those Apparently Immediately Someone Later Before Although
Again Also Many Most Much Other Over Since Such Than Very Yet Within Without Already Whatever However Unless Until Upon Whom Never
Click Select Show Hide Use Return Cancel Okay Ok Note Hint Warning Victory Congratulations Welcome Great Hello Hey Ha Hmm Wow
Look Remember Beware Kill Capture Build Hire Recruit Visit Win Hold Keep Get Gather Collect Make Move Send Help Leave Stop Wait Lord Lady
King Queen Sir Master Duke Baron Count Emperor Empress Father Mother Brother Sister Death Life Light Dark Fire Water Air Earth Hero Heroes
Gold Wood Ore Mercury Crystals Sulfur Gems Attack Defense Spellpower Knowledge Level Experience Morale Luck Mana Day Week Month'''.split())

def natkey(s): return [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', s)]

def load():
    return json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))

def write(name, items, extra=None):
    os.makedirs(CH, exist_ok=True)
    d = {'name': name, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(os.path.join(CH, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def speaker(path):
    m = re.search(r'DialogScene-([^/]*?)-\d+(\.\d+)?\.txt$', path)
    return m.group(1) if m else None

def names():
    it = load()
    names = [i for i in it if i['kind'] == 'name']
    names.sort(key=lambda i: natkey(i['src']))
    have = {i['en'] for i in names}
    k = 0
    for s in range(0, len(names), 380):
        part = names[s:s + 380]
        write('names_%02d' % k, [{'id': i['id'], 'en': i['en'], 'src': i['src']} for i in part]); k += 1
    C = collections.Counter()
    for i in it:
        if i['kind'] != 'text': continue
        for m in re.finditer(r"\b([A-Z][a-z'\-]+(?: (?:of the |of |the |de |von )?[A-Z][a-z'\-]+){0,3})\b", re.sub(r'<[^>]*>', ' ', i['en'])):
            w = m.group(1)
            if w.split()[0] in COMMON or re.search(r"'(s|m|re|ll|ve|d|t)$", w) or w.endswith('-'): continue
            C[w] += 1
    for i in it:
        sp = speaker(i['src'])
        if sp: C[sp] += 3
    pn = [w for w, c in C.most_common() if c >= 3 and w not in have]
    items = [{'id': 'pn%04d' % n, 'en': w} for n, w in enumerate(pn)]
    for s in range(0, len(items), 400):
        write('names_%02d' % k, items[s:s + 400]); k += 1
    print(k, 'name chunks;', len(names), 'names,', len(pn), 'proper nouns')

def gloss_for(text, G, keys):
    out = {}; low = text.lower()
    for k in keys:
        kl = k.lower()
        if kl in low and re.search(r'(?<![a-z])' + re.escape(kl) + r'(?![a-z])', low): out[k] = G[k]
    return out

def group(src):
    p = src.split('/')
    if p[0].lower() in ('maps', 'dialogscenes', 'dialogs'): return '/'.join(p[:3])
    if p[0].lower() == 'text' and len(p) > 2: return '/'.join(p[:3])
    return '/'.join(p[:2])

def content(limit=22000):
    it = load()
    G = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    keys = sorted(G, key=len, reverse=True)
    items = [i for i in it if i['kind'] == 'text']
    items.sort(key=lambda i: natkey(i['src']))
    chunks = []; cur = []; size = 0; curtop = None
    for i in items:
        top = i['src'].split('/')[0].lower()
        if cur and (size + len(i['en']) > limit or top != curtop):
            chunks.append(cur); cur = []; size = 0
        curtop = top
        d = {'id': i['id'], 'en': i['en'], 'src': i['src']}
        sp = speaker(i['src'])
        if sp: d['speaker'] = sp
        cur.append(d); size += len(i['en'])
    if cur: chunks.append(cur)
    n = collections.Counter()
    for c in chunks:
        tag = re.sub(r'[^a-z0-9]+', '_', c[0]['src'].split('/')[0].lower()).strip('_')
        n[tag] += 1
        name = '%s_%02d' % (tag, n[tag])
        write(name, c, {'glossary': gloss_for('\n'.join(x['en'] for x in c), G, keys)})
    print(len(chunks), 'chunks', dict(n))

if __name__ == '__main__':
    {'names': names, 'content': content}[sys.argv[1]]()
