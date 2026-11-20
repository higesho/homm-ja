# Unpack the whole "My Games" part of the Heroes 7.5 Mod setup zip to work/h75/full (for tests; the MSI is not run)
import zipfile, os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
W = os.path.join(ROOT, 'work', 'h75')
P = 'Content/PersonalFolder/My Games/Might & Magic Heroes VII/MMH7Game/'
z = zipfile.ZipFile(os.path.join(W, 'Heroes_7.5_Mod_1.45-SetupFiles.zip'))
n = 0
for i in z.infolist():
    if not i.filename.startswith(P) or i.filename.endswith('/'): continue
    out = os.path.join(W, 'full', *i.filename[len(P):].split('/'))
    if os.path.exists(out) and os.path.getsize(out) == i.file_size: continue
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with z.open(i) as s, open(out, 'wb') as d:
        while True:
            b = s.read(1 << 22)
            if not b: break
            d.write(b)
    n += 1
print(n, 'files unpacked')
