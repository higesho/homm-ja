# texts of the HoMM1 scenario maps: name and description (English slots of the header)
import os, sys, json, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import mp2
MAPS = os.path.join(ROOT, '..', 'MAPS')
def cstr(d, off, n): return d[off:off + n].split(b'\0')[0].decode('latin-1')
def texts(path):
    d = open(path, 'rb').read(); out = []
    if not path.upper().endswith('.MAP'): return out
    n, t = cstr(d, mp2.NAME_OFF, mp2.NAME_LEN), cstr(d, mp2.DESC_OFF, mp2.DESC_LEN)
    if n: out.append(('H:name', 'name', n, mp2.NAME_LEN - 1))
    if t: out.append(('H:desc', 'desc', t, mp2.DESC_LEN - 1))
    return out
def map_files():
    return sorted(glob.glob(os.path.join(MAPS, '*')), key=lambda p: os.path.basename(p).upper())
if __name__ == '__main__':
    items = []
    for f in map_files():
        for key, kind, en, lim in texts(f):
            items.append({'id': 'M:%s:%s' % (os.path.basename(f).upper(), key), 'en': en, 'map': os.path.basename(f), 'kind': kind, 'limit': lim})
    os.makedirs(os.path.join(ROOT, 'text', 'src'), exist_ok=True)
    json.dump(items, open(os.path.join(ROOT, 'text', 'src', 'maps.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(items))
