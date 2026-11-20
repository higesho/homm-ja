# release/HoMM6_JP.zip: Data23.orc + msimg32.dll + 読んでください.txt (UTF-8 BOM, CRLF) + OFL.txt
import os, zipfile, shutil
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
R = os.path.join(ROOT, 'release')
OFL = os.path.join(R, 'OFL.txt')
if not os.path.exists(OFL):
    shutil.copyfile(r'L:\Games\GOG\TocaRaceDriver3\Heroes of Might and Magic V - Tribes of the East\_jp\release\OFL.txt', OFL)
readme = open(os.path.join(R, 'readme_src.txt'), encoding='utf-8-sig').read().replace('\r\n', '\n').replace('\n', '\r\n')
out = os.path.join(R, 'HoMM6_JP.zip')
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.writestr('読んでください.txt', b'\xef\xbb\xbf' + readme.encode('utf-8'))
    z.write(os.path.join(R, 'Data23.orc'), 'Data23.orc', compress_type=zipfile.ZIP_STORED)   # already deflated inside
    z.write(os.path.join(ROOT, 'build', 'msimg32.dll'), 'msimg32.dll')
    z.write(OFL, 'OFL.txt')
print(out, os.path.getsize(out), [i.filename for i in zipfile.ZipFile(out).infolist()])
