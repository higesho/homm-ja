# HoMM3 TXT tokenizer: rows of cells; cells may be quoted (multi-line, "" escapes). Roundtrip-safe.
import re
def parse(b):
    s=b.decode('latin-1'); i=0; n=len(s); rows=[]; row=[]; 
    while i<=n:
        if i==n:
            if row: rows.append((row,'')) 
            break
        if s[i]=='"':
            j=i+1; buf=[]
            while j<n:
                if s[j]=='"':
                    if j+1<n and s[j+1]=='"': buf.append('"'); j+=2; continue
                    break
                buf.append(s[j]); j+=1
            # text after closing quote until tab/eol is kept as tail
            k=j+1; 
            m=re.match(r'[^\t\r\n]*',s[k:]); tail=m.group(); k+=len(tail)
            row.append([''.join(buf),True,tail]); i=k
        else:
            m=re.match(r'[^\t\r\n]*',s[i:]); row.append([m.group(),False,'']); i+=len(m.group())
        if i>=n: rows.append((row,'')); break
        if s[i]=='\t': i+=1; continue
        eol='\r\n' if s.startswith('\r\n',i) else s[i]
        rows.append((row,eol)); row=[]; i+=len(eol)
    return rows
def dump(rows, enc='latin-1'):
    out=[]
    for row,eol in rows:
        cells=[]
        for txt,q,tail in row:
            if q or any(c in txt for c in '\t\r\n"'):
                cells.append('"'+txt.replace('"','""')+'"'+tail)
            else: cells.append(txt+tail)
        out.append('\t'.join(cells)+eol)
    return ''.join(out).encode(enc)
if __name__=='__main__':
    import sys,glob,os
    for p in sys.argv[1:]:
        b=open(p,'rb').read(); r=parse(b)
        ok = dump(r)==b
        print(os.path.basename(p), 'rows',len(r),'roundtrip',ok)
