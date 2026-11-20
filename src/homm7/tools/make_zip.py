# release/HoMM7_JP.zip: the game-folder tree (Binaries, MMH7Game) + 読んでください.txt (UTF-8 BOM, CRLF) + OFL.txt
# release/HoMM7_JP_UCP.zip / HoMM7_JP_H75.zip: our JPN files for UCP 1.3 / Heroes 7.5 Mod 1.45 (tools/modloc.py <mod> build)
import os, zipfile, glob
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
R = os.path.join(ROOT, 'release')
STAGE = os.path.join(ROOT, 'work', 'stage')

def readme(name):
    t = open(os.path.join(R, name), encoding='utf-8-sig').read().replace('\r\n', '\n').replace('\n', '\r\n')
    return b'\xef\xbb\xbf' + t.encode('utf-8')

out = os.path.join(R, 'HoMM7_JP.zip')
files = [('Binaries/Win64/dinput8.dll', os.path.join(ROOT, 'build', 'dinput8.dll'))]
for p in sorted(glob.glob(os.path.join(STAGE, 'CookedPC', 'H7JP', '*.upk'))):
    files.append(('MMH7Game/CookedPC/H7JP/' + os.path.basename(p), p))
for area in ('General', 'Content', 'Maps', 'Duel'):
    for p in sorted(glob.glob(os.path.join(STAGE, area, 'JPN', '*.JPN'))):
        files.append(('MMH7Game/Localization/%s/JPN/%s' % (area, os.path.basename(p)), p))
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.writestr('読んでください.txt', readme('readme_src.txt'))
    z.write(os.path.join(R, 'OFL.txt'), 'OFL.txt')
    for arc, p in files: z.write(p, arc)
print(out, os.path.getsize(out), len(zipfile.ZipFile(out).namelist()), 'files')

for mod, tag in (('ucp', 'UCP'), ('h75', 'H75')):
    out = os.path.join(R, 'HoMM7_JP_%s.zip' % tag)
    U = os.path.join(ROOT, 'work', 'stage_' + mod)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr('読んでください_%s.txt' % tag, readme('readme_%s_src.txt' % mod))
        for p in sorted(glob.glob(os.path.join(U, 'Localization', '*', 'JPN', '*.JPN'))):
            z.write(p, os.path.relpath(p, U).replace(os.sep, '/'))
    print(out, os.path.getsize(out), len(zipfile.ZipFile(out).namelist()), 'files')
