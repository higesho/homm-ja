# check text/out/<chunk>.json against text/chunks/<chunk>.json: missing ids, codes, ASCII double quotes
#   python tools/check_chunk.py [chunk ...]   (no args: all chunks that have an output)
import os, sys, json, re, glob, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from intfile import clean_en
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CODE = re.compile(r'%[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*|</?c>|\\n')

def codes(s):
    return collections.Counter(CODE.findall(s))

def check(name):
    ch = json.load(open(os.path.join(ROOT, 'text', 'chunks', name + '.json'), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'out', name + '.json')
    if not os.path.exists(p): return ['no output']
    try: out = json.load(open(p, encoding='utf-8'))
    except Exception as e: return ['bad json: %s' % e]
    errs = []
    for i in ch['items']:
        i['en'] = clean_en(i['en'])
        ja = out.get(i['id'])
        if ja is None: errs.append('%s missing' % i['id']); continue
        if not isinstance(ja, str): errs.append('%s not a string' % i['id']); continue
        a, b = codes(i['en']), codes(ja)
        if a != b: errs.append('%s codes -%s +%s' % (i['id'], dict(a - b), dict(b - a)))
        if '"' in ja and '"' not in i['en']: errs.append('%s has an ASCII double quote' % i['id'])
        if re.search(r'%[A-Za-z_][\w.]*[A-Za-z0-9]', ja) and re.search(r'%[\w.]+[A-Za-z0-9]', ja):
            for m in re.finditer(r'(%[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)', ja):
                if m.group(1) not in i['en']: errs.append('%s code %s glued to following text' % (i['id'], m.group(1)))
    return errs

if __name__ == '__main__':
    names = sys.argv[1:] or sorted(os.path.basename(p)[:-5] for p in glob.glob(os.path.join(ROOT, 'text', 'out', '*.json')))
    bad = 0
    for n in names:
        e = check(n)
        if e:
            bad += 1
            print(n, len(e), 'problems'); [print('   ', x) for x in e[:15]]
    print(len(names), 'checked,', bad, 'with problems')
