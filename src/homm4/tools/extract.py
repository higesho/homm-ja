# English text to translate -> text/src/items.json ([{id, en, src}]) and text/src/tables.json (table specs)
#  tables/strings: only the display columns below (keyword columns stay English; *.hot_keys rows stay English)
#  maps/campaigns: u16-length strings that are not resource names or identifiers
import os, sys, re, json, pickle, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import h4r, tables, maps
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

COLS = {   # file -> (first data row, display columns)
    'table.Adventure Object.h4d': (1, [4]),
    'table.Artifacts.h4d': (2, [2, 3, 7, 8, 9]),
    'table.buildings.h4d': (1, [1, 9]),
    'table.creature_abilities.h4d': (1, [1, 2]),
    'table.creatures.h4d': (2, [1, 2, 3, 4, 26, 27]),
    'table.heroes.h4d': (1, [1, 4]),
    'table.Interface.h4d': (1, [2, 3]),
    'table.random_names.h4d': (1, [1]),
    'table.skills.h4d': (1, [1, 2]),
    'table.Spells.h4d': (2, [1, 2, 33, 34, 35, 36]),
    'strings.Text.h4d': (0, [1]),
    'strings.intro voice over text.h4d': (1, [1]),
}

def sig(rows):
    return '\x1f'.join(rows[0]) + '\x1e' + '\x1f'.join(rows[1] if len(rows) > 1 else [])

def is_text(s):
    if not re.search(r'[A-Za-z]{2}', s): return False
    if s in ('not applicable',): return False
    return True

def main():
    items = collections.OrderedDict()   # en -> src
    specs = []
    for arc, name, rows in tables.files():
        if name not in COLS: continue
        first, cols = COLS[name]
        specs.append({'file': arc + ':' + name, 'sig': sig(rows), 'first': first, 'cols': cols})
        for i, r in enumerate(rows[first:], first):
            if r and r[0].endswith('.hot_keys'): continue
            for c in cols:
                if c < len(r) and is_text(r[c]) and r[c] not in items:
                    items[r[c]] = name.split('.')[1].replace(' ', '_') + ':%d' % c
    # maps
    names = set()
    for a in ['heroes4', 'storm', 'x2', 'updates', 'storm_override', 'x2_override']:
        d, E = h4r.entries(os.path.join(maps.GAME, 'data', a + '.h4r'))
        for e in E:
            n = e['name'][:-4] if e['name'].endswith('.h4d') else e['name']
            parts = n.split('.')
            for i in range(len(parts)):
                for j in range(i + 1, len(parts) + 1): names.add('.'.join(parts[i:j]).lower())
    ident = re.compile(r'^[a-z0-9_ .\-\']+$')
    mapitems = collections.OrderedDict(); mapall = set()
    for fname, b in maps.files():
        tag = re.sub(r'[^A-Za-z0-9]+', '_', os.path.splitext(os.path.basename(fname.split(':')[-1]))[0]).strip('_')
        if ':' in fname: tag = 'camp_' + tag
        for m in maps.members(b):
            for p, s in maps.scan(m):
                if ident.match(s) or s.lower() in names: continue
                if '_' in s and ' ' not in s: continue
                if '\\' in s or re.search(r'\.(h4d|h4c|bmp|wav|txt)$', s, re.I): continue
                if not is_text(s): continue
                mapall.add(s)
                if s not in items and s not in mapitems: mapitems[s] = 'map:' + tag
    out = []
    for en, src in items.items(): out.append({'id': 'u%05d' % len(out), 'en': en, 'src': src})
    for en, src in mapitems.items(): out.append({'id': 'm%05d' % len(out), 'en': en, 'src': src})
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(out, open(os.path.join(ROOT, 'text', 'src', 'items.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    json.dump(specs, open(os.path.join(ROOT, 'text', 'src', 'tables.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    json.dump(sorted(mapall), open(os.path.join(ROOT, 'text', 'src', 'mapset.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    c = collections.Counter(i['src'].split(':')[0] for i in out)
    print(len(out), dict(c), sum(len(i['en']) for i in out), 'chars')

if __name__ == '__main__':
    main()
