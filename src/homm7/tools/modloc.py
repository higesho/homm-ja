# Japanese for mods that install into Documents\My Games\Might & Magic Heroes VII\MMH7Game\ (CookedPC, Localization\<area>\INT)
#   python tools/modloc.py <mod> chunks : strings of the mod's INT files that have no translation yet -> text/chunks/<mod>_NN.json
#   python tools/modloc.py <mod> build  : JPN files for the mod's loc files -> work/stage_<mod>/Localization/<area>/JPN/*.JPN
#   mods: ucp = Unofficial Community Patch 1.3 (work/ucp/x), h75 = Heroes 7.5 Mod 1.45 (work/h75/x)
# The game reads that folder before its own, so our JPN files for the mod go there too and replace the plain-game ones.
import os, sys, json, re, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from intfile import ROOT, read_text, parse, is_text
import chunk as C

MOD = sys.argv[1] if len(sys.argv) > 2 else 'ucp'
UCP = os.path.join(ROOT, 'work', MOD, 'x', 'Localization')
STAGE = os.path.join(ROOT, 'work', 'stage_' + MOD, 'Localization')
NAMES_FIRST = MOD == 'h75'
SKIP = {'unrealed.int', 'h7mapeditor.int', 'wwiseconfig.int', 'gfxui.int'}

def ucp_files():
    for area in sorted(os.listdir(UCP)):
        d = os.path.join(UCP, area, 'INT')
        if not os.path.isdir(d): continue
        for f in sorted(os.listdir(d), key=str.lower):
            if f.lower().endswith('.int') and f.lower() not in SKIP:
                yield area, f, os.path.join(d, f)

def chunks(limit=20000):
    ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
    G = json.load(open(os.path.join(ROOT, 'text', 'glossary.json'), encoding='utf-8'))
    keys = sorted(G, key=len, reverse=True)
    from intfile import LOC
    seen = {}; n = 0
    for area, fn, path in ucp_files():
        base = os.path.splitext(fn)[0]
        van = {}
        vd = os.path.join(LOC, area, 'INT')
        vf = [x for x in os.listdir(vd) if x.lower() == fn.lower()] if os.path.isdir(vd) else []
        if vf:
            for i, sec, key, v, q, parts in parse(read_text(os.path.join(vd, vf[0]))): van[(sec, key.strip())] = v
        for i, sec, key, v, q, parts in parse(read_text(path)):
            if not is_text(v) or v in ja or v in seen: continue
            if key.strip() == 'CREDITS' or sec == '[Hints.A_LoadscreenHints H7LoadingHints]': continue   # stay English
            it = {'id': '%s%05d' % (MOD[0], n), 'en': v, 'src': '%s/%s:%s %s' % (area, base, sec, key.strip())}; n += 1
            old = van.get((sec, key.strip()))
            if old and old in ja:      # UCP rewrote a text we already translated: show the old pair
                it['old_en'] = old; it['old_ja'] = ja[old]
            seen[v] = it
    items = list(seen.values())
    if NAMES_FIRST and not any(os.path.basename(p).startswith('names_%s_' % MOD) for p in os.listdir(C.CH)):
        # first pass: short names (creatures, abilities, heroes...) -> names_<mod>_NN; merge.py turns them into glossary
        from items import NAME_KEYS
        def is_name(it):
            k = it['src'].rsplit(' ', 1)[-1]
            return NAME_KEYS.match(k) and len(it['en']) <= 40 and '\\n' not in it['en'] and not it['en'].rstrip().endswith('.')
        nm = [it for it in items if is_name(it)]
        for k in range(0, len(nm), 400):
            C.write('names_%s_%02d' % (MOD, k // 400), [{'id': i['id'], 'en': i['en'], 'src': i['src']} for i in nm[k:k + 400]])
        print(len(nm), 'names in', (len(nm) + 399) // 400, 'name chunks; run merge.py after translating them, then chunks again')
        return
    parts = []; cur = []; size = 0
    for it in items:
        if cur and size + len(it['en']) > limit:
            parts.append(cur); cur = []; size = 0
        cur.append(it); size += len(it['en'])
    if cur: parts.append(cur)
    for k, c in enumerate(parts):
        C.write('%s_%02d' % (MOD, k), c, {'glossary': C.gloss_for('\n'.join(x['en'] for x in c), G, keys)})
    print(len(items), 'strings in', len(parts), 'chunks')

def build():
    import build as B
    ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
    if os.path.exists(STAGE): shutil.rmtree(STAGE)
    n_tr = n_all = nf = 0
    for area, fn, path in ucp_files():
        out = []; cur = None
        for i, sec, key, v, q, parts in parse(read_text(path)):
            if not is_text(v): continue
            n_all += 1
            if sec in B.KEEP_ENGLISH: continue
            t = ja.get(v)
            if t is None: continue
            n_tr += 1
            if sec != cur:
                if out: out.append('')
                out.append(sec); cur = sec
            out.append('%s="%s"' % (parts[1].strip(), t))
        if not out: continue
        d = os.path.join(STAGE, area, 'JPN'); os.makedirs(d, exist_ok=True)
        open(os.path.join(d, os.path.splitext(fn)[0] + '.JPN'), 'wb').write(b'\xff\xfe' + ('\r\n'.join(out) + '\r\n').encode('utf-16le'))
        nf += 1
    print(MOD, 'lines translated %d / %d in %d files' % (n_tr, n_all, nf))

if __name__ == '__main__':
    {'chunks': chunks, 'build': build}[sys.argv[-1]]()
