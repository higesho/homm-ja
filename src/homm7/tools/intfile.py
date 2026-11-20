# Reading/writing Heroes VII localization .int files (UE3 ini; UTF-16LE, UTF-8 or cp1252)
import os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
GAME = os.path.dirname(ROOT)
LOC = os.path.join(GAME, 'MMH7Game', 'Localization')
AREAS = ['General', 'Content', 'Maps', 'Duel']
SKIP_FILES = {'unrealed.int', 'h7mapeditor.int', 'wwiseconfig.int', 'gfxui.int'}   # editor / config, not game text

def read_text(p):
    b = open(p, 'rb').read()
    if b[:2] == b'\xff\xfe': return b[2:].decode('utf-16le')
    if b[:2] == b'\xfe\xff': return b[2:].decode('utf-16be')      # some Heroes 7.5 Mod files
    if b[:3] == b'\xef\xbb\xbf': return b[3:].decode('utf-8')
    try: return b.decode('utf-8')
    except UnicodeDecodeError: return b.decode('cp1252')

def int_files():
    """(area, filename, path) of every INT file with game text"""
    out = []
    for a in AREAS:
        d = os.path.join(LOC, a, 'INT')
        for f in sorted(os.listdir(d), key=str.lower):
            if f.lower().endswith('.int') and f.lower() not in SKIP_FILES:
                out.append((a, f, os.path.join(d, f)))
    return out

LINE = re.compile(r'^(\s*)([^=\[;][^=]*?)(\s*=\s*)(.*?)(\s*)$')
IDENT = re.compile(r'^(H7SeqAct_\w+|\[DEPRECATED\??\]|DEPRECATED|\[NOT USED - DO NOT TRANSLATE\]|https?://\S+|%[\w.]+)$')

def is_text(v):
    return bool(re.search(r'[A-Za-z]', v)) and not IDENT.match(v.strip())

def parse(text):
    """yield (lineno, section, key, value, quoted, parts) for every key=value line;
    parts = (indent, key, eq, rawvalue, trail)"""
    sec = ''
    for i, line in enumerate(text.split('\n')):
        s = line.rstrip('\r')
        st = s.strip()
        if st.startswith('['):
            sec = st; continue
        m = LINE.match(s)
        if not m: continue
        raw = m.group(4)
        if len(raw) >= 2 and raw[0] == '"' and raw[-1] == '"':
            v, q = raw[1:-1], True
        else:
            v, q = raw, False
        yield i, sec, m.group(2), v, q, m.groups()

def clean_en(s):
    """English text read from a UTF-16BE file before read_text knew that encoding (NULs between letters,
    the value's quotes and CR kept): the real text"""
    if '\x00' not in s: return s
    s = s.replace('\x00', '').strip()
    if s.endswith('\r'): s = s[:-1].rstrip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"': s = s[1:-1]
    return s
