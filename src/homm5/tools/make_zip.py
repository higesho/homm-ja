# release/HoMM5_*_JP.zip: one zip per game (pak in the game folder layout + readme + OFL), each under 10 MB
import os, zipfile
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
R = os.path.join(ROOT, 'release')
ZIPS = {
    'HoMM5_TotE_JP.zip': ('data/H5JP_TotE.pak', 'tote/H5JP_TotE.pak'),
    'HoMM5_JP.zip': ('data/H5JP_H5.pak', 'h5/H5JP_H5.pak'),
    'HoMM5_HoF_JP.zip': ('dataa1/a1p1-sound.pak', 'hof/a1p1-sound.pak'),
}
readme = open(os.path.join(R, 'readme_src.txt'), encoding='utf-8-sig').read().replace('\r\n', '\n').replace('\n', '\r\n')
for name, (arc, src) in ZIPS.items():
    out = os.path.join(R, name)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr('読んでください.txt', b'\xef\xbb\xbf' + readme.encode('utf-8'))
        z.write(os.path.join(R, src), arc, compress_type=zipfile.ZIP_STORED)   # the pak is already compressed
        z.write(os.path.join(R, 'OFL.txt'), 'OFL.txt')
    print(name, os.path.getsize(out), [i.filename for i in zipfile.ZipFile(out).infolist()])
