# check text/out/<chunk>.json against text/chunks/<chunk>.json: missing ids, % codes, \n count, EUC-JP encodability
#   python tools/check_chunk.py [chunk ...]   (no args: all chunks that have an output)
import os, sys, json, re, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CODE = re.compile(r'<[^<>]*>')

def norm(s): return s

def codes(s):
    return sorted(CODE.findall(s))

def check(name, verbose=True):
    ch = json.load(open(os.path.join(ROOT, 'text', 'chunks', name + '.json'), encoding='utf-8'))
    p = os.path.join(ROOT, 'text', 'out', name + '.json')
    if not os.path.exists(p): return ['no output']
    try: out = json.load(open(p, encoding='utf-8'))
    except Exception as e: return ['bad json: %s' % e]
    errs = []
    for i in ch['items']:
        ja = out.get(i['id'])
        if ja is None: errs.append('%s missing' % i['id']); continue
        if not isinstance(ja, str): errs.append('%s not a string' % i['id']); continue
        if codes(i['en']) != codes(ja): errs.append('%s codes %s -> %s' % (i['id'], codes(i['en']), codes(ja)))
        if i['en'].count('\n') != ja.count('\n'): errs.append('%s newlines %d -> %d' % (i['id'], i['en'].count('\n'), ja.count('\n')))
    return errs

if __name__ == '__main__':
    names = sys.argv[1:] or sorted(os.path.basename(p)[:-5] for p in glob.glob(os.path.join(ROOT, 'text', 'out', '*.json')))
    bad = 0
    for n in names:
        e = check(n)
        if e:
            bad += 1
            print(n, len(e), 'problems'); [print('   ', x) for x in e[:12]]
    print(len(names), 'checked,', bad, 'with problems')
