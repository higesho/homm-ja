# Build Data23.orc: every English workbook with the "Translated text" column replaced from text/ja.json,
# the English font config pointing Fontin/creditsFont at our Japanese TTF, and the TTF.
#   python tools/build.py [out.orc]        env H6JP_MARK=1 : prefix untranslated rows with "▲" (test)
#   The "Text to translate" (English) and "Description" columns are left empty: the game keys rows by identifier.
import os, sys, json, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orcfs, orcpack, book, make_font, extract
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
FONT = 'H6JP\\H6JPSerif.ttf'
FONT_XML = '''<?xml version="1.0" encoding="utf-8"?>
<Fonts>
''' + ''.join('\t<Font name="%s" style="%s" file="%s"/>\n' % (n, s, FONT) for n in ('Fontin', 'creditsFont') for s in ('', 'Bold', 'Italic')) + '</Fonts>\n'

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'release', 'Data23.orc')
    ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
    mark = os.environ.get('H6JP_MARK')
    files = {}; used = set(); n = t = 0
    for name, e in sorted(extract.latest().items()):
        rs = extract.rows(e.arc.read(e))
        res = []
        for r in rs[1:]:
            r = (r + [''] * 5)[:5]
            if r[0] in ja:
                r[1] = ja[r[0]]; t += 1
            elif mark and r[0].strip() and re.search('[A-Za-z]{2}', r[0]):
                r[1] = '▲' + r[1]
            if not os.environ.get('H6JP_KEEPSRC'):
                r[0] = ''; r[4] = ''        # no original English text or developer notes in the distribution
            n += 1; used.update(r[1])
            res.append(r)
        files[name] = book.make(res)
    chars = make_font.base_chars(level1=True) | {c for c in used if ord(c) >= 0x20}
    files['SourceAssets/Font/English (United States) {0409}.xml'] = FONT_XML.encode('utf-8')
    files['SourceAssets/Font/' + FONT.replace('\\', '/')] = make_font.make_font(chars)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    size = orcpack.pack(files, out)
    print(out, size, 'bytes;', t, 'of', n, 'rows translated;', len(chars), 'glyphs')

if __name__ == '__main__':
    main()
