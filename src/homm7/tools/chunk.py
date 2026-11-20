# translation chunks -> text/chunks/*.json
#   python tools/chunk.py names    : names_* (kind=name items + capitalised proper nouns of the story texts)
#   python tools/chunk.py content  : the rest (kind=text), grouped by file, with a per-chunk glossary (text/glossary.json)
# Names that Heroes VI already translated (its text/glossary.json) are attached as "h6" hints.
import os, sys, re, json, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CH = os.path.join(ROOT, 'text', 'chunks')
H6G = r'L:\SteamLibrary\steamapps\common\Might and Magic Heroes VI\_jp\text\glossary.json'
COMMON = set('''The You Your This That If When It As With After But And In On At For To Of We He She They His Her Their There What Who
Why How Now Then So Once Only Do Does Did Are Is Was Were Be Will Would Can Could Should May Might Must Not No Yes Oh Well Good Thank
Please Here All Any Some Each Every One Two Three Four Five Six Seven Eight Nine Ten My Perhaps Even Soon Bring Unfortunately Give Lose
Just Defeat Finally Don Let An By These Take Meanwhile Today Maybe Find Or Tell Come Still Sorry From Day Our While Ah Everyone Instead
Have Though Where More Something Uh Suddenly Having Yeah First Go Because Those Apparently Immediately Someone Later Before Although
Again Also Many Most Much Other Over Since Such Than Very Yet Within Without Already Whatever However Unless Until Upon Whom Never
Click Select Show Hide Use Return Cancel Okay Ok Note Hint Warning Victory Congratulations Welcome Great Hello Hey Ha Hmm Wow
Look Remember Beware Kill Capture Build Hire Recruit Visit Win Hold Keep Get Gather Collect Make Move Send Help Leave Stop Wait Lord Lady
King Queen Sir Master Duke Baron Count Emperor Empress Father Mother Brother Sister Death Life Light Dark Fire Water Air Earth Hero Heroes
Gold Wood Ore Crystal Crystals Attack Defense Defence Level Experience Morale Luck Mana Week Month Turn Turns Allows Gives Increases Reduces
Deals Grants Target Creatures Creature Enemy Enemies Allied Ally Friendly Might Magic Damage Health Initiative Movement Your Total
Aye Nay Lord Lords Mistress Prince Princess Brother Sisters Mother Father Son Daughter Uncle Aunt Cousin God Gods Talk Reach Destroy
Protect Escort Explore Survive Conquer Eliminate Lead Bring Free Rescue Speak Meet Follow Clear Ensure Prevent Retrieve Obtain'''.split())

def natkey(s): return [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', s)]

def load():
    return json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))

def write(name, items, extra=None):
    os.makedirs(CH, exist_ok=True)
    d = {'name': name, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(os.path.join(CH, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def h6():
    try: return json.load(open(H6G, encoding='utf-8'))
    except Exception: return {}

def names():
    it = load()
    H6 = h6(); H6l = {k.lower(): v for k, v in H6.items()}
    nm = [i for i in it if i['kind'] == 'name']
    nm.sort(key=lambda i: (natkey(i['src'].split(':')[0]), i['n']))
    have = {i['en'] for i in nm}
    def item(i, idk='id'):
        d = {'id': i[idk], 'en': i['en']}
        if 'src' in i: d['src'] = i['src']
        if i['en'].lower() in H6l: d['h6'] = H6l[i['en'].lower()]
        return d
    k = 0
    for s in range(0, len(nm), 400):
        write('names_%02d' % k, [item(i) for i in nm[s:s + 400]]); k += 1
    C = collections.Counter()
    for i in it:
        if i['kind'] != 'text': continue
        for m in re.finditer(r"\b([A-Z][a-z'\-]+(?: (?:of the |of |the |de |von )?[A-Z][a-z'\-]+){0,3})\b", re.sub(r'<[^>]*>|%[\w.]+', ' ', i['en'])):
            w = m.group(1)
            if w.split()[0] in COMMON or re.search(r"'(s|m|re|ll|ve|d|t)$", w) or w.endswith('-'): continue
            C[w] += 1
    pn = [w for w, c in C.most_common() if c >= 3 and w not in have]
    items = [item({'id': 'pn%04d' % n, 'en': w}) for n, w in enumerate(pn)]
    for s in range(0, len(items), 450):
        write('names_%02d' % k, items[s:s + 450]); k += 1
    print(k, 'name chunks;', len(nm), 'names,', len(pn), 'proper nouns,', sum(1 for i in nm if i['en'].lower() in H6l), 'with h6 hints')

def gloss_for(text, G, keys):
    out = {}; low = text.lower()
    for k in keys:
        kl = k.lower()
        if kl in low and re.search(r'(?<![a-z])' + re.escape(kl) + r'(?![a-z])', low): out[k] = G[k]
    return out

def group(i):
    f = i['src'].split(':')[0]          # e.g. Content/Creature_Abbot
    area, base = f.split('/', 1)
    b = base.lower()
    if area == 'General': return 'ui'
    if area == 'Duel': return 'duel'
    if area == 'Maps':
        if b.startswith('cinematic'): return 'cine'
        return 'map_' + re.sub(r'[^a-z0-9]+', '_', b).strip('_')
    for pre in ('creature_', 'buildings_', 'h7townbuilding', 'h7hero', 'h7items', 'h7cr', 'h7abilit', 'h7spell', 'h7skill', 'h7calendar', 'losttales'):
        if b.startswith(pre): return 'c_' + pre.rstrip('_')
    return 'c_misc'

def content(limit=20000):
    it = load()
    G = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    keys = sorted(G, key=len, reverse=True)
    items = [i for i in it if i['kind'] == 'text']
    by = collections.OrderedDict()
    for i in items:                     # file order is kept: dialogue lines stay in sequence
        by.setdefault(group(i), []).append(i)
    # small map groups are merged so chunks are not tiny
    merged = collections.OrderedDict(); small = []
    for g, lst in by.items():
        if g.startswith('map_') and sum(len(x['en']) for x in lst) < limit // 3:
            small.extend(lst)
        else:
            merged[g] = lst
    if small: merged['map_misc'] = small
    n = 0
    for g, lst in merged.items():
        cur = []; size = 0; parts = []
        for i in lst:
            if cur and size + len(i['en']) > limit:
                parts.append(cur); cur = []; size = 0
            cur.append({'id': i['id'], 'en': i['en'], 'src': i['src']}); size += len(i['en'])
        if cur: parts.append(cur)
        if len(parts) > 1 and sum(len(x['en']) for x in parts[-1]) < limit // 4:
            last = parts.pop(); parts[-1] += last
        for k, c in enumerate(parts):
            name = '%s_%02d' % (g, k)
            write(name, c, {'glossary': gloss_for('\n'.join(x['en'] for x in c), G, keys)}); n += 1
    print(n, 'content chunks')

if __name__ == '__main__':
    {'names': names, 'content': content}[sys.argv[1]]()
