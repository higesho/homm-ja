# heuristic extraction of length-prefixed strings from h3m (gzip) / h3c (gzip-concatenated maps)
import gzip, zlib, struct, re, sys, glob, os, io
def gunzip_all(b):
    out=[]; 
    while b:
        d=zlib.decompressobj(31)
        try: out.append(d.decompress(b))
        except Exception: break
        b=d.unused_data
    return out
OK=set(range(0x20,0x7f))|{9,10,13}|set(range(0x80,0x100))
def strings(d, minlen=1):
    res=[]; i=0; n=len(d)
    while i+4<=n:
        L=struct.unpack_from('<I',d,i)[0]
        if 1<=L<=30000 and i+4+L<=n:
            s=d[i+4:i+4+L]
            if all(c in OK for c in s) and re.search(rb'[A-Za-z]',s) and not (s[0] in b' ' and L==1):
                # avoid tiny false positives: need >=2 letters or a known short word
                if L>=2 or s.isalpha():
                    res.append((i,s.decode('latin-1'))); i+=4+L; continue
        i+=1
    return res
if __name__=='__main__':
    tot=0; n=0
    for p in sys.argv[1:]:
        for f in glob.glob(p):
            for k,d in enumerate(gunzip_all(open(f,'rb').read())):
                ss=[s for _,s in strings(d) if not re.search(r'\.(def|msk|h3m)$',s,re.I)]
                c=sum(len(s) for s in ss); tot+=c; n+=len(ss)
                print(os.path.basename(f),k,len(d),len(ss),c)
    print('TOTAL strings',n,'chars',tot)
