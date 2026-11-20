# check text/out/<chunk>.json against text/chunks/<chunk>.json: missing ids, codes, line breaks
#   python tools/check_chunk.py [chunk ...]   (no args: all chunks that have an output)
import os, sys, json, re, glob, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
CODE = re.compile(r"%\{\d+\}\{\d+\}%t|\{[^{}\s]*\}|</?[A-Za-z_][A-Za-z_0-9]*(?:\s+[A-Za-z_]+\s*=\s*(?:'[^']*'|\"[^\"]*\"|[^\s>]+))*\s*/?>|&[a-z]+;|\\n|\n|^/[a-z]+")

def codes(s):
    return sorted(c.lower() if c.startswith('<') else c for c in CODE.findall(s))

def check(name):
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
        a, b = collections.Counter(codes(i['en'])), collections.Counter(codes(ja))
        if a != b: errs.append('%s codes -%s +%s' % (i['id'], dict(a - b), dict(b - a)))
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
