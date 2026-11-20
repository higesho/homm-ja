# text/src/rows.json -> text/src/items.json : one item per unique English string
#   {id, en, src, kind}  kind = name (short names/titles) | text
#   src = "<area>:<text identifier>" of the first occurrence (area = Master, Flash, Game, Cutscene, <campaign map>, MP/<map>)
import os, sys, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def area(f):
    p = f.split('/')
    if p[1] == 'Scenario':
        m = re.sub(r'v1\.\d$', '', p[3])
        return ('MP/' if p[2] == 'Multiplayer' else '') + m
    return {'Game.orc': 'Game'}.get(p[1], p[1])

NAME_ID = re.compile(r'(\.Name|\.LongName|\.Rank|TownName|HeroName|\.TabName|^@HeroClass\..*|Faction\.\w+|Resource\.\w+|Race\.\d+\.Name)$')

def is_text(en):
    t = re.sub(r'<[^>]*>|\{[^}]*\}', '', en).strip()
    return bool(re.search(r'[A-Za-z]{2}', t))

def main():
    R = json.load(open(os.path.join(ROOT, 'text', 'src', 'rows.json'), encoding='utf-8'))
    seen = {}; items = []
    # base campaign maps before their v1.x copies, so src points at the base name
    order = sorted(range(len(R)), key=lambda i: (0 if not re.search(r'v1\.\d/', R[i]['file']) else 1, i))
    for i in sorted(order, key=lambda i: i):
        pass
    for i in order:
        r = R[i]; en = r['en']
        if not en.strip() or en in seen: continue
        if not is_text(en): continue
        a = area(r['file'])
        kind = 'name' if (NAME_ID.search(r['id']) and len(en) <= 48 and not re.search(r'[.!?]\s*$', en.strip())) else 'text'
        it = {'id': 't%05d' % len(items), 'en': en, 'src': '%s:%s' % (a, r['id']), 'kind': kind, 'n': i}
        if r['desc']: it['note'] = r['desc'][:200]
        seen[en] = it; items.append(it)
    json.dump(items, open(os.path.join(ROOT, 'text', 'src', 'items.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    nn = [x for x in items if x['kind'] == 'name']
    print(len(items), 'items;', len(nn), 'names', sum(len(x['en']) for x in nn), 'chars;',
          sum(len(x['en']) for x in items if x['kind'] == 'text'), 'text chars')

if __name__ == '__main__':
    main()
