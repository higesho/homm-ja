# Build the JPN localization files -> work/stage/<area>/JPN/<file>.JPN (UTF-16LE with BOM, CRLF)
#   python tools/build.py            translations from text/ja.json (untranslated strings stay English)
#   python tools/build.py --pseudo   test mode: "日"+English for short strings, Japanese filler for long ones
import os, sys, json, re, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from intfile import ROOT, LOC, AREAS, int_files, read_text, parse, is_text

STAGE = os.path.join(ROOT, 'work', 'stage')
JPFONT = 'H7JP Sans'                       # our font (tools/make_fonts.py -> CookedPC/H7JP/H7JPFont.upk)
BOLD = {'BoldFont', 'TitleFont', 'H7UbiFontBold', 'H7FontinBold'}
# The loading-screen hints are drawn with the bitmap font MMH7Font_INT (chosen by script, inside H7Components),
# which has no Japanese glyphs: they stay English.
KEEP_ENGLISH = {'[Hints.A_LoadscreenHints H7LoadingHints]'}
CODE = re.compile(r'%[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*|</?c>|\\n')

def pseudo(v):
    if len(v) < 60: return '日' + v
    codes = CODE.findall(v)
    fill = 'これは日本語の折り返しを確かめるための試験の文です。'
    n = max(1, len(re.sub(CODE, '', v)) // 3)
    body = (fill * (n // len(fill) + 1))[:n]
    step = max(1, len(body) // (len(codes) + 1))
    out = ''
    for k, c in enumerate(codes):
        out += body[k * step:(k + 1) * step] + c
    return out + body[len(codes) * step:]

def gfxui():
    """GFxUI.JPN: our font library and every font name of the INT file mapped to our font (other sections fall back to INT)"""
    t = read_text(os.path.join(LOC, 'General', 'INT', 'GFxUI.int'))
    keys = re.findall(r'^((?:Normal|Bold|Small|Title)Font|H7\w+)=', t, flags=re.M)
    out = ['[FontLib]', 'FontLib=H7JPFont.lib.fonts_jp', '', '[Fonts]']
    out += ['%s=%s%s' % (k, JPFONT, ',Bold' if k in BOLD else '') for k in keys]
    m = re.search(r'^MoviePath=(\S+)', t, flags=re.M)          # the IME window movie (a path, kept as in INT)
    if m: out += ['', '[IME]', 'MoviePath=' + m.group(1)]
    return '\r\n'.join(out) + '\r\n'

def build(tr):
    """Each JPN file holds only the section headers and the translated keys: anything missing is read
    from the INT file by the game, so no original English text is shipped."""
    for a in AREAS:
        d = os.path.join(STAGE, a)
        if os.path.exists(d): shutil.rmtree(d)
    n_tr = n_all = n_files = 0
    for area, fn, path in int_files():
        text = read_text(path)
        out = []; cur = None
        for i, sec, key, v, q, parts in parse(text):
            if not is_text(v): continue
            n_all += 1
            if sec in KEEP_ENGLISH: continue
            ja = tr(v)
            if ja is None: continue
            n_tr += 1
            if sec != cur:
                if out: out.append('')
                out.append(sec); cur = sec
            out.append('%s="%s"' % (parts[1].strip(), ja))
        if not out: continue
        d = os.path.join(STAGE, area, 'JPN'); os.makedirs(d, exist_ok=True)
        open(os.path.join(d, os.path.splitext(fn)[0] + '.JPN'), 'wb').write(b'\xff\xfe' + ('\r\n'.join(out) + '\r\n').encode('utf-16le'))
        n_files += 1
    d = os.path.join(STAGE, 'General', 'JPN'); os.makedirs(d, exist_ok=True)
    open(os.path.join(d, 'GFxUI.JPN'), 'wb').write(b'\xff\xfe' + gfxui().encode('utf-16le'))
    print('lines translated %d / %d in %d files' % (n_tr, n_all, n_files))

if __name__ == '__main__':
    if '--pseudo' in sys.argv:
        build(pseudo)
    else:
        ja = json.load(open(os.path.join(ROOT, 'text', 'ja.json'), encoding='utf-8'))
        build(lambda v: ja.get(v))
