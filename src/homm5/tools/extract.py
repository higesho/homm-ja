# English text of HoMM5 Tribes of the East + HoMM5 / Hammers of Fate (GOG) -> text/src/items.json
#   every *.txt in the texts paks (UTF-16). One item per unique string; "src" = first path, "paths" = all paths.
#   kind: name (names of things, for the glossary), text (everything translated), keep (identifiers, tags only),
#         nbsp (shown as is, only its spaces become NBSP: credits, key names)
import os, sys, re, json, zipfile, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
G = os.path.abspath(os.path.join(ROOT, '..', '..'))
TOTE = os.path.join(G, 'Heroes of Might and Magic V - Tribes of the East')
BASE = os.path.join(G, 'Heroes of Might and Magic V')
PAKS = [(TOTE, 'data/texts.pak'), (TOTE, 'data/a2p1-texts.pak'),
        (BASE, 'data/texts.pak'), (BASE, 'data/p3-texts.pak'), (BASE, 'dataa1/texts.pak'), (BASE, 'dataa1/p3-texts.pak'),
        (BASE, 'dataa1/a1-texts.pak'), (BASE, 'dataa1/a1p1-texts.pak'), (BASE, 'datals/p5-texts.pak'), (BASE, 'datals/p6-texts.pak')]

def decode(b):
    if b[:2] == b'\xff\xfe': return b[2:].decode('utf-16-le', 'replace')
    if b[:3] == b'\xef\xbb\xbf': return b[3:].decode('utf-8', 'replace')
    try: return b.decode('utf-16-le') if len(b) % 2 == 0 and b[1:2] == b'\0' else b.decode('cp1252')
    except UnicodeDecodeError: return b.decode('cp1252', 'replace')

GROUPS = [PAKS[:2], PAKS[2:4] + PAKS[8:10], PAKS[2:8]]   # TotE, HoMM5, Hammers of Fate (each: newest file per path)

def all_texts():
    """(game, path lower) -> (path, text, date)"""
    out = {}
    for g, group in enumerate(GROUPS):
        for game, p in group:
            z = zipfile.ZipFile(os.path.join(game, p))
            for i in z.infolist():
                n = i.filename
                if not n.lower().endswith('.txt'): continue
                k = (g, n.lower())
                if k in out and out[k][2] >= i.date_time: continue
                out[k] = (n, decode(z.read(n)), i.date_time)
    return out

NAME_RE = re.compile(r'(^|/)(name|[^/]*_name|caption|header)\.txt$', re.I)

def kind(path, s):
    p = path.lower()
    if 'ui/consts/mltags/' in p or re.search(r'(cam-set-name|name-camera)[^/]*\.txt$', p) or 'format' in p.rsplit('/', 1)[-1]: return 'keep'
    if 'ui/consts/inputkeys/' in p: return 'nbsp'
    bare = re.sub(r'<[^>]*>', '', s)
    if p.startswith(('maps/test/', 'test/')): return 'skip'
    # used as folder / file names or URLs by the program: must stay exactly as shipped
    if p in ('ui/apptitle.txt', 'ui/product_name.txt', 'ui/ubicom-account-site.txt', 'ui/consts/profiles/defaultplayername.txt',
             'ui/default_player1_name.txt', 'ui/default_player2_name.txt') or 'filename_format' in p or re.search(r'\.sav|https?://', s): return 'keep'
    if p.startswith(('dialogs/cameras', 'dialogcameraset', 'lights/')): return 'keep'
    if not re.search(r'[A-Za-zЀ-ӿ]{2}', bare): return 'keep' if ' ' not in s else 'nbsp'
    if p.startswith('text/game/') and NAME_RE.search(p): return 'name'
    return 'text'

def main():
    T = all_texts()
    items = collections.OrderedDict(); kinds = collections.Counter()
    for k in sorted(T):
        path, s, _ = T[k]
        if not s: continue
        kd = kind(path, s); kinds[kd] += 1
        if kd == 'skip': continue
        it = items.get(s)
        if it is None:
            items[s] = it = {'en': s, 'src': path, 'kind': kd, 'paths': []}
        elif kd == 'name' and it['kind'] == 'text': it['kind'] = 'name'
        elif kd == 'text' and it['kind'] in ('keep', 'nbsp'): it['kind'] = 'text'
        it['paths'].append(path)
    out = []
    for i, it in enumerate(items.values()):
        it['id'] = 's%05d' % i; out.append(it)
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(out, open(os.path.join(ROOT, 'text', 'src', 'items.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    c = collections.Counter(); n = collections.Counter()
    for it in out: c[it['kind']] += len(it['en']); n[it['kind']] += 1
    print(len(T), 'files', dict(kinds), '| unique', dict(n), 'chars', dict(c))

if __name__ == '__main__':
    main()
