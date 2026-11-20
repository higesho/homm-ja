# text/src/exe.json: the translatable strings of HEROES2.EXE (unique English texts, in data order)
import os, sys, json, re
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from classify import cat

# functions whose string arguments are never shown in the game window
INTERNAL_CALLS = {'dword ptr [0x32f64]', '0x955e6', '0x94699', '0x947b8', '0x95671', '0x95787', '0x94ca9', '0xbb653',
                  '0xaf5b0', '0xaf664', '0xaf686', '0x98d34', '0xa0595', '0x85751', '0x971c3', '0xb7101',
                  '0xca80c', '0x91d26', '0x92594', '0x97f72', '0x9664f', '0x81482', '0x97dbd', '0x95cac', '0xb6c6e',
                  '0x98acf', '0x93964'}
DEBUG = re.compile(r'(\r\n|AIL_|^\[|%X|0x%|Manager|manager|^In |Start$|End\s*$|^Bad |^DO |\bRMT\b|\.cpp|\.h\b|assert|<=|==|!=|->|\bNULL\b|'
                   r'Transmit|CheckBuy|^Result|exception|destructor|virtual|terminate|constructor)')

def load():
    R = json.load(open(os.path.join(ROOT, 'work', 'strings.json')))
    C = json.load(open(os.path.join(ROOT, 'work', 'ctx.json')))
    return R, C

def translatable(r, C):
    s = r['s']
    if not r['nref'] or r['mid']: return False
    c = cat(s)
    if c in ('file', 'fmt'): return False
    users = C.get(hex(r['off']), [])
    if users and all(u in INTERNAL_CALLS for u in users): return False
    if DEBUG.search(s): return False
    if c == 'token':
        return True               # judged per word in skip_words
    if re.fullmatch(r'[A-Z0-9_]{1,10}', s): return False       # codes (LG1, RSG1, PLYREXIT)
    if re.fullmatch(r'[A-Za-z0-9_]+[A-Z][a-z]+[A-Za-z0-9_]*', s) and ' ' not in s and len(s) > 12: return False   # identifiers
    return True

if __name__ == '__main__':
    R, C = load()
    skip_words = set(json.load(open(os.path.join(ROOT, 'text', 'skip_words.json'), encoding='utf-8')))
    seen = {}; items = []
    for r in R:
        if not translatable(r, C) or r['s'] in skip_words: continue
        if r['s'] in seen: seen[r['s']]['offs'].append(hex(r['off'])); continue
        it = {'id': 'E:%x' % r['off'], 'en': r['s'], 'offs': [hex(r['off'])]}
        seen[r['s']] = it; items.append(it)
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(items, open(os.path.join(ROOT, 'text', 'src', 'exe.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(items), sum(len(i['en']) for i in items))
