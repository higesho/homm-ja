# text/out/*.json (agent output) -> text/ja.json {english: japanese}; checks format codes, braces, line breaks
#   python tools/merge.py [--glossary]   (--glossary also writes text/glossary.md from the names chunks)
import os, sys, json, re, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
T = os.path.join(ROOT, 'text')
FMT = re.compile(r'%[-+#0]*\d*(?:\.\d+)?[hlL]?[sdiucxXf%]')

def problems(en, ja):
    p = []
    if FMT.findall(en) != FMT.findall(ja): p.append('format %s -> %s' % (FMT.findall(en), FMT.findall(ja)))
    if en.count('{') != ja.count('{') or en.count('}') != ja.count('}'): p.append('braces')
    if en.count('\n') != ja.count('\n'): p.append('newlines %d -> %d' % (en.count('\n'), ja.count('\n')))
    try: ja.encode('euc_jp')
    except UnicodeEncodeError as e: p.append('not EUC: %r' % ja[e.start:e.end])
    if re.search(r'[｡-ﾟ]', ja): p.append('half-width kana')
    return p

def keep_english(en):
    # technical strings the agents must not change
    return bool(re.fullmatch(r'[\w.*\\:%/ -]*\.[\w*]{1,3}', en) or re.fullmatch(r'[A-Z0-9 _]+', en) or 'AT ' in en[:3])

def main():
    out = json.load(open(os.path.join(T, "prefill.json"), encoding="utf-8")) if os.path.exists(os.path.join(T, "prefill.json")) else {}
    bad = []; missing = []
    fixes = json.load(open(os.path.join(T, 'fixes.json'), encoding='utf-8')) if os.path.exists(os.path.join(T, 'fixes.json')) else {}
    names = []
    for ch in sorted(glob.glob(os.path.join(T, 'chunks', '*.json'))):
        c = json.load(open(ch, encoding='utf-8'))
        of = os.path.join(T, 'out', c['name'] + '.json')
        if not os.path.exists(of): missing.append(c['name']); continue
        o = json.load(open(of, encoding='utf-8'))
        for it in c['items']:
            en = it['en']; ja = fixes.get(en, o.get(it['id']))
            if ja is None: bad.append((c['name'], it['id'], en, None, ['missing'])); continue
            if keep_english(en): ja = en
            ja = ja.replace('－', '-').replace('―', 'ー').replace('～', '〜')
            pr = problems(en, ja)
            if pr and en not in fixes: bad.append((c['name'], it['id'], en, ja, pr)); continue
            out[en] = ja
            if c['kind'] == 'names': names.append((en, ja))
    json.dump(out, open(os.path.join(T, 'ja.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    with open(os.path.join(T, 'merge_problems.txt'), 'w', encoding='utf-8') as f:
        for b in bad: f.write('%s %s %r\n   -> %r  %s\n' % b)
    print('ok', len(out), 'problems', len(bad), 'missing chunks', missing)
    if '--glossary' in sys.argv:
        with open(os.path.join(T, 'glossary.md'), 'w', encoding='utf-8') as f:
            f.write('# HoMM2 用語集（名前・ラベル。翻訳済み。この訳語を使うこと）\n\n| English | 日本語 |\n|---|---|\n')
            for en, ja in names:
                if en != ja: f.write('| %s | %s |\n' % (en.replace('|', '/'), ja.replace('|', '/')))
        print('glossary', len(names))

if __name__ == '__main__':
    main()
