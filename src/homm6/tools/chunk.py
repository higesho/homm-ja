# translation chunks -> text/chunks/*.json
#   python tools/chunk.py names    : names_* (kind=name items + capitalised proper nouns of the story texts)
#   python tools/chunk.py content  : the rest (kind=text), grouped by area, with a per-chunk glossary (text/glossary.json)
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
Gold Wood Ore Crystal Crystals Attack Defense Level Experience Morale Luck Mana Week Month Turn Turns Allows Gives Increases Reduces
Deals Grants Target Creatures Creature Enemy Enemies Allied Ally Friendly Might Magic Damage Health Initiative Movement Your Total
Aye Nay Lord Lords Mistress Prince Princess Brother Sisters Mother Father Son Daughter Uncle Aunt Cousin God Gods'''.split())

def natkey(s): return [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', s)]

def load():
    return json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))

def write(name, items, extra=None):
    os.makedirs(CH, exist_ok=True)
    d = {'name': name, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(os.path.join(CH, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def names():
    it = load()
    nm = [i for i in it if i['kind'] == 'name']
    nm.sort(key=lambda i: (i['src'].split(':')[0] != 'Game', natkey(i['src'].split(':')[1].split('.')[0]), i['n']))
    have = {i['en'] for i in nm}
    k = 0
    for s in range(0, len(nm), 400):
        write('names_%02d' % k, [{'id': i['id'], 'en': i['en'], 'src': i['src']} for i in nm[s:s + 400]]); k += 1
    C = collections.Counter()
    for i in it:
        if i['kind'] != 'text': continue
        for m in re.finditer(r"\b([A-Z][a-z'\-]+(?: (?:of the |of |the |de |von )?[A-Z][a-z'\-]+){0,3})\b", re.sub(r'<[^>]*>|\{[^}]*\}', ' ', i['en'])):
            w = m.group(1)
            if w.split()[0] in COMMON or re.search(r"'(s|m|re|ll|ve|d|t)$", w) or w.endswith('-'): continue
            C[w] += 1
    pn = [w for w, c in C.most_common() if c >= 3 and w not in have]
    items = [{'id': 'pn%04d' % n, 'en': w} for n, w in enumerate(pn)]
    for s in range(0, len(items), 450):
        write('names_%02d' % k, items[s:s + 450]); k += 1
    print(k, 'name chunks;', len(nm), 'names,', len(pn), 'proper nouns')

def gloss_for(text, G, keys):
    out = {}; low = text.lower()
    for k in keys:
        kl = k.lower()
        if kl in low and re.search(r'(?<![a-z])' + re.escape(kl) + r'(?![a-z])', low): out[k] = G[k]
    return out

def group(i):
    a, ident = i['src'].split(':', 1)
    if a == 'Game': return 'game_' + re.sub(r'[^a-z]+', '', ident.split('.')[0].lower())
    if a in ('Master', 'Flash', 'Cutscene'): return 'ui_' + a.lower()
    if a.startswith('MP/'): return 'mp'
    return 'map_' + re.sub(r'[^a-z0-9]+', '_', a.lower()).strip('_')

def content(limit=20000):
    it = load()
    G = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    keys = sorted(G, key=len, reverse=True)
    items = [i for i in it if i['kind'] == 'text']
    # keep game-file order inside each group; small Game.orc groups go together
    by = collections.OrderedDict()
    for i in sorted(items, key=lambda i: i['n']):
        g = group(i)
        if g.startswith('game_') and g not in ('game_ability', 'game_entitytype', 'game_buildingtype', 'game_achievement', 'game_avatarslot', 'game_neutralhero'):
            g = 'game_misc'
        by.setdefault(g, []).append(i)
    n = 0
    for g, lst in by.items():
        if g.startswith('map_') or g == 'mp':
            lst.sort(key=lambda i: i['n'])
        cur = []; size = 0; k = 0
        parts = []
        for i in lst:
            if cur and size + len(i['en']) > limit:
                parts.append(cur); cur = []; size = 0
            d = {'id': i['id'], 'en': i['en'], 'src': i['src']}
            if 'note' in i: d['note'] = i['note']
            cur.append(d); size += len(i['en'])
        if cur: parts.append(cur)
        # merge a tiny last part into the previous one
        if len(parts) > 1 and sum(len(x['en']) for x in parts[-1]) < limit // 4:
            last = parts.pop(); parts[-1] += last
        for k, c in enumerate(parts):
            name = '%s_%02d' % (g, k)
            write(name, c, {'glossary': gloss_for('\n'.join(x['en'] for x in c), G, keys)}); n += 1
    print(n, 'content chunks')

if __name__ == '__main__':
    {'names': names, 'content': content}[sys.argv[1]]()
