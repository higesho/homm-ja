# text/chunks/*.json for the translator agents: names first (glossary), then the rest in data order
import os, sys, json, re
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
CH = os.path.join(ROOT, 'text', 'chunks')

def is_name(s):
    return (len(s) <= 40 and '\n' not in s and '%' not in s and not re.search(r'[.?!:]\s*$', s)
            and len(s.split()) <= 6 and re.search(r'[A-Za-z]', s) is not None)

def write(name, kind, items, extra=None):
    d = {'name': name, 'kind': kind, 'items': items}
    if extra: d.update(extra)
    json.dump(d, open(os.path.join(CH, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

if __name__ == '__main__':
    os.makedirs(CH, exist_ok=True)
    items = json.load(open(os.path.join(ROOT, 'text', 'src', 'exe.json'), encoding='utf-8'))
    names = [i for i in items if is_name(i['en'])]
    rest = [i for i in items if not is_name(i['en'])]
    k = 0
    for a in range(0, len(names), 150):
        k += 1; write('names_%02d' % k, 'names', [{'id': i['id'], 'en': i['en']} for i in names[a:a + 150]])
    k = 0; cur = []; size = 0
    for i in rest:
        cur.append({'id': i['id'], 'en': i['en']}); size += len(i['en'])
        if size > 3500 or len(cur) >= 60:
            k += 1; write('exe_%03d' % k, 'content', cur); cur = []; size = 0
    if cur: k += 1; write('exe_%03d' % k, 'content', cur)
    print('names', len(names), 'chunks', (len(names) + 149) // 150, 'content chunks', k, 'items', len(rest))

def map_chunks():
    items = json.load(open(os.path.join(ROOT, 'text', 'src', 'maps.json'), encoding='utf-8'))
    seen = set(); cur = []; size = 0; k = 0
    for i in items:
        if i['en'] in seen: continue
        seen.add(i['en'])
        it = {'id': i['id'], 'en': i['en'], 'map': i['map'], 'kind': i['kind']}
        if i['limit']: it['max_chars'] = i['limit'] // 2
        if cur and (size + len(i['en']) > 4000 or cur[-1]['map'] != i['map'] and size > 2500):
            k += 1; write('map_%03d' % k, 'map', cur); cur = []; size = 0
        cur.append(it); size += len(i['en'])
    if cur: k += 1; write('map_%03d' % k, 'map', cur)
    print('map chunks', k, 'items', len(seen))
