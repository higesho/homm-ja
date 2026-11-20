import sys, os, re
sys.path.insert(0, os.path.dirname(__file__))
from upk import Pkg
k = Pkg(sys.argv[1]); out = sys.argv[2]; flt = sys.argv[3] if len(sys.argv) > 3 else ''
os.makedirs(out, exist_ok=True)
for e in k.exports:
    if k.clsname(e) != 'SwfMovie' or flt not in e['name']: continue
    b = k.d[e['off']:e['off'] + e['size']]
    m = re.search(rb'(FWS|CWS|GFX|CFX)[\x05-\x20]', b)
    pos = m.start() if m else -1
    import struct
    ln = struct.unpack_from('<I', b, pos + 4)[0] if pos >= 0 else 0
    print(e['name'], 'size', e['size'], 'sig', m.group(1) if m else None, 'at', pos, 'len', ln, 'arraylen?', struct.unpack_from('<i', b, pos - 4)[0] if pos >= 4 else None)
    open(os.path.join(out, e['name'] + '.' + (m.group(1).decode().lower() if m else 'bin')), 'wb').write(b[pos:] if pos >= 0 else b)
    open(os.path.join(out, e['name'] + '.head'), 'wb').write(b[:pos] if pos > 0 else b'')
