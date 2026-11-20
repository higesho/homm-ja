# Extract from the Heroes 7.5 Mod setup zip what the translation work needs (no installer is run):
#   work/h75/x/Localization/...   (all loc files), work/h75/x/CookedPC/GUI/*, CookedPC/*.u, readme as text
import zipfile, os, re, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
W = os.path.join(ROOT, 'work', 'h75')
Z = os.path.join(W, 'Heroes_7.5_Mod_1.45-SetupFiles.zip')
P = 'Content/PersonalFolder/My Games/Might & Magic Heroes VII/MMH7Game/'
z = zipfile.ZipFile(Z)
n = 0
for i in z.infolist():
    if not i.filename.startswith(P) or i.filename.endswith('/'): continue
    r = i.filename[len(P):]
    if r.startswith('Localization/') or r.startswith('CookedPC/GUI/') or re.match(r'CookedPC/[^/]+\.u$', r) or r.startswith('CookedPC/Flash/'):
        out = os.path.join(W, 'x', *r.split('/'))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with z.open(i) as s, open(out, 'wb') as d:
            while True:
                b = s.read(1 << 20)
                if not b: break
                d.write(b)
        n += 1
print(n, 'files extracted')
rtf = z.read('readme_1.45.rtf').decode('cp1252', 'replace')
t = re.sub(r'\\par[d]?\b', '\n', rtf)
t = re.sub(r"\\'([0-9a-f]{2})", lambda m: bytes([int(m.group(1), 16)]).decode('cp1252', 'replace'), t)
t = re.sub(r'\{\\\*[^{}]*\}', '', t)
t = re.sub(r'\\[a-zA-Z]+-?\d* ?', '', t)
t = t.replace('{', '').replace('}', '')
open(os.path.join(W, 'readme.txt'), 'w', encoding='utf-8').write(t)
