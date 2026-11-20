# Japanese texts + fonts -> build/<game>/... and the .pak files
#   text/out/*.json (id -> ja) merged in text/ja.json by merge.py
#   For every text file of a game: translated -> Japanese with break spaces; otherwise -> spaces become NBSP
#   (the fonts draw the ASCII space with zero width: it is the line-break point between Japanese characters)
#   python tools/build.py [tote|h5|hof ...]
import os, sys, re, json, shutil, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract, make_fonts, make_pak
ROOT = extract.ROOT
GAMES = {
    'tote': (extract.TOTE, ['data/texts.pak', 'data/a2p1-texts.pak'], 'data/H5JP_TotE.pak', (2026, 1, 1, 0, 0, 0)),
    # HoMM5 and Hammers of Fate both load data\*.pak (newest file wins): one pak with the newest text of every file
    'h5':   (extract.BASE, ['data/texts.pak', 'data/p3-texts.pak', 'datals/p5-texts.pak', 'datals/p6-texts.pak', 'dataa1/texts.pak',
                            'dataa1/p3-texts.pak', 'dataa1/a1-texts.pak', 'dataa1/a1p1-texts.pak'], 'data/H5JP_H5.pak', (2026, 1, 1, 0, 0, 0)),
}
NOSTART = set('、。，．・：；？！ー）」』】〕］｝〉》ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ…々〜～!?)]},.:;%')
NOEND = set('（「『【〔［｛〈《([{')

def wide(c): o = ord(c); return o >= 0x2e80 or 0xff00 <= o <= 0xffef

def jp_text(s):
    """Japanese text -> engine text: real spaces -> NBSP, zero-width break spaces between Japanese characters"""
    out = []
    parts = re.split(r'(<[^<>]*>)', s)
    prev = ''   # last visible char (across tags)
    for p in parts:
        if p.startswith('<') and p.endswith('>'): out.append(p); continue
        for c in p:
            if c == ' ': c = ' '
            if prev and c not in '\r\n' and prev not in '\r\n' and (wide(c) or wide(prev)) \
               and c not in NOSTART and prev not in NOEND and c != ' ' and prev != ' ':
                out.append(' ')
            out.append(c); prev = c
    return ''.join(out)

def en_text(s):
    """English left as is: short labels keep full-width spaces (NBSP); long texts keep breakable ASCII spaces"""
    if len(re.sub(r'<[^<>]*>', '', s)) > 40: return s
    parts = re.split(r'(<[^<>]*>)', s)
    return ''.join(p if p.startswith('<') else p.replace(' ', ' ') for p in parts)

def game_texts(game, paks):
    out = {}
    for p in paks:
        z = zipfile.ZipFile(os.path.join(game, p))
        for i in z.infolist():
            if not i.filename.lower().endswith('.txt'): continue
            k = i.filename.lower()
            if k in out and out[k][2] >= i.date_time: continue
            out[k] = (i.filename, extract.decode(z.read(i.filename)), i.date_time)
    return out

def build(key, ja, items_by_en):
    game, paks, outpak, stamp = GAMES[key]
    T = game_texts(game, paks)
    d = os.path.join(ROOT, 'build', key); os.makedirs(d, exist_ok=True)
    files = {}
    chars = set(); n = {'ja': 0, 'nbsp': 0, 'keep': 0}
    for k, (path, s, _) in T.items():
        if not s: continue
        it = items_by_en.get(s)
        kd = extract.kind(path, s)
        if kd in ('keep', 'skip'): n['keep'] += 1; continue
        t = ja.get(s)
        if t and kd in ('text', 'name'): out = jp_text(t); n['ja'] += 1
        else:
            if ' ' not in s: n['keep'] += 1; continue
            if re.search(r'[A-Za-z]{3}', re.sub(r'<[^<>]*>', '', s)) and kd != 'nbsp': n['untranslated'] = n.get('untranslated', 0) + 1; continue   # never ship original prose
            out = en_text(s); n['nbsp'] += 1
        chars |= set(re.sub(r'<[^<>]*>', '', out))
        files[path] = b'\xff\xfe' + out.encode('utf-16-le')
    cs = os.path.join(d, 'charset.txt')
    open(cs, 'w', encoding='utf-8').write(''.join(sorted(c for c in chars if c >= ' ')))
    font_pak = [os.path.join(game, p) for p in paks]
    files.update(make_fonts.main(font_pak, cs, None))
    os.makedirs(os.path.join(ROOT, 'release', key), exist_ok=True)
    out = os.path.join(ROOT, 'release', key, os.path.basename(outpak))
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for a in sorted(files):
            zi = zipfile.ZipInfo(a, date_time=stamp); zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi, files[a])
    print(out, os.path.getsize(out))
    if key == 'h5':
        # Hammers of Fate prefers its dataa1 paks over data\*.pak (texts and fonts): the same pak again as the last
        # pak of its fixed dataa1 list (a1p1-sound.pak, absent from the GOG install)
        hof = os.path.join(ROOT, 'release', 'hof'); os.makedirs(hof, exist_ok=True)
        for f in os.listdir(hof): os.remove(os.path.join(hof, f))
        fp = os.path.join(hof, 'a1p1-sound.pak')
        shutil.copyfile(out, fp)
        print(fp, os.path.getsize(fp))
    print(key, n, len(chars), 'chars')

if __name__ == '__main__':
    ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
    items = json.load(open(os.path.join(ROOT, 'text', 'src', 'items.json'), encoding='utf-8'))
    by = {i['en']: i for i in items}
    for k in (sys.argv[1:] or ['tote', 'h5']): build(k, ja, by)
