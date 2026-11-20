# extract translatable units -> text/src/txt_units.json, text/src/map_units.json
import sys, os, re, json, glob, hashlib
sys.path.insert(0, os.path.dirname(__file__))
from txt import parse
from mapstr import gunzip_all, strings
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXT=os.path.join(ROOT,'ext'); GAME=os.path.dirname(ROOT)
SKIPF={'objects','objtmplt','rand_trn','rmg','cranim','walls','editor','editrcmd','cmpeditr','cmpedcmd',
       'serial','building','ballist','movement','heroes','cmpmovie','cmpmusic','credits'}
# file -> (columns or None for all, header rows to skip)
RULES={'artraits':({0,21},2),'campdiag':({1},0),'camphigh':({1,2},0),'crbanks':({0},2),
       'crtraits':({0,1,23},2),'hctraits':({0},2),'help':({1},1),'hotraits':({0},2),'regions':({0},0),
       'seerhut':({1,2,3,4,5,6},1),'sptraits':({0,1,29,30,31,32},2),'sstraits':({0,1,2,3},1),
       'herospec':({0,1,2},0),'genrltxt':({0},1)}
NEVER={'NEWGAME','AUTOSAVE','PLYREXIT','ping','HumanCPU','HandiText','AGRText','NewLoadSave','Army Speed Names',
       'Town Size Names','Moat Names'}
def keep_en(v):
    s=v.strip()
    if not re.search(r'[A-Za-z]{2}',s): return True
    if s in NEVER: return True
    if s.startswith('//') or s.startswith('Note: Do not change') or s.startswith('Insert all data before this line'): return True
    if re.fullmatch(r'[gc][A-Z]\w+', s) or re.fullmatch(r'[a-z]+(_[a-z]+)+', s): return True
    if s.endswith(' Window'): return True
    if '_' in s and re.fullmatch(r'[A-Za-z0-9_]+,?', s): return True     # JkText section headings
    if re.fullmatch(r'[A-Za-z0-9_]+\.(def|pcx|wav|smk|bik|h3m|h3c|txt|fnt|msk)',s,re.I): return True
    if re.fullmatch(r'[A-Z0-9_ |]+',s) and '_' in s: return True
    return False
def txt_sources():
    files={}
    for lod in ('H3ab_bmp','H3bitmap'):     # H3bitmap (SoD/Complete) is the one the game uses
        for p in glob.glob(os.path.join(EXT,lod,'*.txt'))+glob.glob(os.path.join(EXT,lod,'*.TXT')):
            files[os.path.basename(p).lower()]=p
    return files
def txt_units():
    out=[]
    for low,p in sorted(txt_sources().items()):
        stem=low[:-4]
        if stem in SKIPF: continue
        cols,hdr=RULES.get(stem,(None,0))
        if stem=='help':   # col0 = status-bar rollover text, except heading rows (col0 == col1 or col1 empty)
            rows=parse(open(p,'rb').read())
            for ri,(row,eol) in enumerate(rows):
                if ri<1 or len(row)<2: continue
                c0,c1=row[0][0],row[1][0]
                if not c1.strip() or c0.strip()==c1.strip() or keep_en(c0) or re.fullmatch(r'[A-Z0-9_ ]+', c0.strip()): continue
                out.append({'id':f'{low}:{ri}:0','en':c0})
        rows=parse(open(p,'rb').read())
        for ri,(row,eol) in enumerate(rows):
            if ri<hdr: continue
            for ci,(t,q,tail) in enumerate(row):
                if cols is not None and ci not in cols: continue
                if keep_en(t): continue
                if stem=='help' and t.strip()==row[0][0].strip(): continue
                out.append({'id':f'{low}:{ri}:{ci}','en':t})
    return out
def map_sources():
    srcs=[]
    for p in sorted(glob.glob(os.path.join(GAME,'Maps','*.h3m'))+glob.glob(os.path.join(GAME,'Maps','*.tut'))): srcs.append(('map',p))
    seen=set()
    for lod in ('H3ab_bmp','H3bitmap'):
        for p in sorted(glob.glob(os.path.join(EXT,lod,'*.h3c'))+glob.glob(os.path.join(EXT,lod,'*.H3C'))):
            b=os.path.basename(p).lower()
            if b in seen: continue
            seen.add(b); srcs.append(('camp',p))
    return srcs
def map_units():
    out={}; order=[]
    for kind,p in map_sources():
        for k,d in enumerate(gunzip_all(open(p,'rb').read())):
            for off,s in strings(d):
                if re.search(r'\.(def|msk|h3m)$',s,re.I): continue
                if len(s.strip())<2: continue
                h=hashlib.sha1(s.encode('latin-1')).hexdigest()[:12]
                if h not in out:
                    out[h]={'id':'M:'+h,'en':s,'src':[]}; order.append(h)
                src=f'{os.path.basename(p)}#{k}'
                if src not in out[h]['src']: out[h]['src'].append(src)
    return [out[h] for h in order]
if __name__=='__main__':
    t=txt_units(); m=map_units()
    json.dump(t,open(os.path.join(ROOT,'text/src/txt_units.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=0)
    json.dump(m,open(os.path.join(ROOT,'text/src/map_units.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=0)
    print('txt units',len(t),sum(len(u['en']) for u in t))
    print('map units',len(m),sum(len(u['en']) for u in m))
