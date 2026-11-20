# build/txt.bin (translated TXT files) + build/tr.bin (map/campaign strings, FNV64 -> EUC-JP)
import sys, os, json, struct, re, glob
sys.path.insert(0, os.path.dirname(__file__))
from txt import parse, dump
from extract import txt_sources
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def load_ja(name):
    p=os.path.join(ROOT,'text','ja',name)
    return json.load(open(p,encoding='utf-8')) if os.path.exists(p) else {}
REPL={'～':'〜','‐':'－','—':'―','−':'－','〜':'〜','･':'・','｡':'。','\u00a0':' ','“':'「','”':'」','‘':'\'','’':'\'','…':'…','！':'！','•':'・','剝':'剥','–':'－','é':'e','è':'e'}
def euc(s):
    s=''.join(REPL.get(c,c) for c in s)
    out=bytearray()
    for c in s:
        try: out+=c.encode('euc_jp')
        except UnicodeEncodeError: out+=b'?' ; print('  no EUC for', repr(c), file=sys.stderr)
    return bytes(out)
def fnv(b):
    h=0xcbf29ce484222325
    for x in b: h=((h^x)*0x100000001b3)&0xffffffffffffffff
    return h
def build_txt(ja):
    files=txt_sources(); per={}
    for k,v in ja.items():
        f,r,c=k.rsplit(':',2); per.setdefault(f,{})[(int(r),int(c))]=v
    ents=[]; blob=b''
    for f,m in sorted(per.items()):
        rows=parse(open(files[f],'rb').read())
        for (r,c),v in m.items(): rows[r][0][c][0]=euc(v).decode('latin-1')
        data=dump(rows)
        name=os.path.basename(files[f]).encode()
        ents.append((name,len(blob),len(data))); blob+=data
    hdr=b'H3TX'+struct.pack('<I',len(ents)); base=8+32*len(ents)
    for n,o,l in ents: hdr+=n.ljust(24,b'\0')[:24]+struct.pack('<II',base+o,l)
    return hdr+blob, len(ents)
def build_txt_hota(ja):
    per = {}
    for k, v in ja.items():
        f, r, c = k[2:].rsplit(':', 2); per.setdefault(f, {})[(int(r), int(c))] = v
    files = {os.path.basename(p).lower(): p for p in glob.glob(os.path.join(ROOT, 'ext', 'hota_txt', '*'))}
    ents = []; blob = b''
    for f, m in sorted(per.items()):
        rows = parse(open(files[f], 'rb').read())
        for (r, c), v in m.items(): rows[r][0][c][0] = euc(v).decode('latin-1')
        data = dump(rows)
        ents.append((os.path.basename(files[f]).encode(), len(blob), len(data))); blob += data
    hdr = b'H3TX' + struct.pack('<I', len(ents)); base = 8 + 32 * len(ents)
    for n, o, l in ents: hdr += n.ljust(24, b'\0')[:24] + struct.pack('<II', base + o, l)
    return hdr + blob, len(ents)
def build_tr(ja, units):
    en={u['id']:u['en'] for u in units}; tab={}
    for k,v in ja.items():
        if k not in en or not v: continue
        b=en[k].encode('latin-1'); tab[fnv(b)]=euc(v)
    keys=sorted(tab); blob=b''; ents=b''
    for h in keys:
        ents+=struct.pack('<QII',h,0,0)
    base=8+16*len(keys); ents=b''
    for h in keys:
        ents+=struct.pack('<QII',h,len(blob),len(tab[h])); blob+=tab[h]+b'\0'
    return b'H3TR'+struct.pack('<I',len(keys))+ents+blob, len(keys)
if __name__=='__main__':
    tj=load_ja('txt.json'); mj=load_ja('map.json')
    units=json.load(open(os.path.join(ROOT,'text/src/map_units.json'),encoding='utf-8'))
    hp=os.path.join(ROOT,'text/src/hota_map_units.json')
    if os.path.exists(hp): units+=json.load(open(hp,encoding='utf-8'))
    b,n=build_txt_hota(load_ja('hota_txt.json')); open(os.path.join(ROOT,'build','txt_hota.bin'),'wb').write(b); print('hota txt files',n)
    b,n=build_txt(tj); open(os.path.join(ROOT,'build','txt.bin'),'wb').write(b); print('txt files',n,len(tj))
    b,n=build_tr(mj,units); open(os.path.join(ROOT,'build','tr.bin'),'wb').write(b); print('map strings',n)
