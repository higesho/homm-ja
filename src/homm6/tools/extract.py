# Collect the newest English localization workbooks (Data.orc .. Data22.orc, later archive wins) -> text/src/
#   text/src/files.json : path -> archive
#   text/src/rows.json  : [{file, id, en, cat, desc}]
import os, sys, json, re, glob
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orcfs
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
NS = {'ss': 'urn:schemas-microsoft-com:office:spreadsheet'}
EN = 'English (United States) {0409}.xml'

def main_orcs():
    out = []; i = 0
    while True:
        p = os.path.join(orcfs.GAME, 'Data%s.orc' % (i or ''))
        if not os.path.exists(p) or i == 23: break   # Data23.orc = our patch
        out.append(p); i += 1
    return out

def rows(data):
    root = ET.fromstring(data)
    res = []
    for r in root.iter('{urn:schemas-microsoft-com:office:spreadsheet}Row'):
        cells = []
        for c in r.findall('ss:Cell', NS):
            idx = c.get('{urn:schemas-microsoft-com:office:spreadsheet}Index')
            if idx:
                while len(cells) < int(idx) - 1: cells.append('')
            d = c.find('ss:Data', NS)
            cells.append(''.join(d.itertext()) if d is not None else '')
        res.append(cells)
    return res

def latest():
    win = {}
    for p in main_orcs():
        o = orcfs.Orc(p)
        for e in o.entries:
            if e.name.endswith(EN) and '/PreviousExport/' not in e.name and e.name.startswith('Localization/'):
                win[e.name] = e
    return win

if __name__ == '__main__':
    win = latest()
    os.makedirs(os.path.join(ROOT, 'text', 'src', 'orig'), exist_ok=True)
    allrows = []; files = {}
    for name, e in sorted(win.items()):
        d = e.arc.read(e)
        files[name] = os.path.basename(e.arc.path)
        rs = rows(d)
        hdr = rs[0] if rs else []
        n = 0
        for r in rs[1:]:
            r = r + [''] * (5 - len(r))
            if not r[2] and not r[0]: continue
            allrows.append({'file': name, 'en': r[0], 'tr': r[1], 'id': r[2], 'cat': r[3], 'desc': r[4]}); n += 1
        print('%-80s %-12s %6d rows %s' % (name, files[name], n, '' if hdr[:1] == ['Text to translate'] else 'HDR?' + repr(hdr[:3])))
    json.dump(files, open(os.path.join(ROOT, 'text', 'src', 'files.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    json.dump(allrows, open(os.path.join(ROOT, 'text', 'src', 'rows.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    uniq = {r['en'] for r in allrows if r['en'].strip()}
    print(len(allrows), 'rows', len(uniq), 'unique', sum(len(u) for u in uniq), 'chars', sum(1 for r in allrows if r['en'] != r['tr']), 'en!=tr')
