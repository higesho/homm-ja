# text/src/exe.json: the translatable strings of HEROES2.EXE (unique English texts, in data order)
import os, sys, json, re
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from classify import cat

# functions whose string arguments are never shown in the game window
INTERNAL_CALLS = {'0xa19db', '0x84234', '0x13620', '0x83b59', '0x8a868', '0x88710', '0x8d828', '0x873b8', '0x8aa7c',
                  '0x8b82e', '0x8a518', '0x9e7a4', '0x24824', '0x825e4', '0x8beb4', '0x588a0', '0x118f8', '0x84256',
                  '0x136a8', '0x8a95c', '0x13834', '0x8b890', '0x2405c', '0x8a748', '0x87568', '0xad261', '0x136f8',
                  '0x876a8', '0x8f60c'}
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
