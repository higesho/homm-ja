# split the referenced data strings into: file names, lowercase tokens, format-only, text
import json, re, os, sys, random
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = json.load(open(os.path.join(ROOT, 'work', 'strings.json')))
EXT = r'(icn|82m|bin|til|pal|xmi|smk|mse|cpp|hpp|h|mp2|mx2|agg|fnt|bmp|wav|hs|gm1|cfg|exe|txt|dat|h2c|hxc|c|cmp|gmx|sav|\*|%s|ini|log|mdm|map|pcx)'
FILE = re.compile(r'^[\w~\-\\./:%*?]*\.' + EXT + r'$', re.I)
def cat(s):
    if FILE.match(s): return 'file'
    if re.fullmatch(r'[a-z0-9_ ]+', s): return 'token'
    if not re.search(r'[A-Za-z]{2,}', s): return 'fmt'
    return 'text'
if __name__ == '__main__':
    ref = [r for r in R if r['nref']]
    cats = {}
    for r in ref: cats.setdefault(cat(r['s']), []).append(r)
    for k, v in cats.items(): print(k, len(v), sum(len(r['s']) for r in v))
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    for r in random.sample(cats['text'], 50): print(hex(r['off']), repr(r['s'][:90]))
    print('TOKENS', [r['s'] for r in cats['token']][:400])
