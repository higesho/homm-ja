# Collect every translatable string of the INT files -> text/src/items.json
#   item: {id t#####, en, src "<area>/<file>:<section> <key>", kind name|text, n (occurrences)}
import os, sys, json, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from intfile import ROOT, int_files, read_text, parse, is_text

NAME_KEYS = re.compile(r'^(mName|mObstacleName|mTownNames\[\d+\]|mFactionName|mFactionFortName|mFactionArmyName|mResourceName|mArchetypeName|mHeroName|mTitle)$')

def main():
    seen = collections.OrderedDict()
    for area, fn, path in int_files():
        base = os.path.splitext(fn)[0]
        for i, sec, key, v, q, parts in parse(read_text(path)):
            if not is_text(v): continue
            src = '%s/%s:%s %s' % (area, base, sec, key.strip())
            it = seen.get(v)
            if it is None:
                kind = 'name' if (NAME_KEYS.match(key.strip()) and len(v) <= 40 and '\\n' not in v and not v.rstrip().endswith('.')) else 'text'
                seen[v] = it = {'en': v, 'src': src, 'kind': kind, 'n': 0}
            it['n'] += 1
    items = []
    for k, it in enumerate(seen.values()):
        items.append(dict(id='t%05d' % k, **it))
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(items, open(os.path.join(ROOT, 'text', 'src', 'items.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    c = collections.Counter(i['kind'] for i in items)
    print(len(items), 'items', dict(c), sum(len(i['en']) for i in items), 'chars')

if __name__ == '__main__':
    main()
