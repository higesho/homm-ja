import sys, struct, capstone
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from le import LE
L = LE(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'HEROES.EXE'))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
fixmap = {}
for (oi, so, st, fl, to, toff, pi) in L.fix:
    fixmap[L.va(oi, so)] = L.va(to - 1, toff)
def cstr(va):
    oi = L.obj_of(va)
    if oi is None: return None
    m = L.mem[oi]; o = va - L.objs[oi]['base']; e = m.find(b'\0', o)
    s = m[o:e]
    if 0 < len(s) < 200 and all(32 <= c < 127 or c in (10, 13) for c in s): return s.decode()
    return None
def dis(va, n=40):
    m = L.mem[0]; o = va - L.objs[0]['base']
    for i in md.disasm(bytes(m[o:o + n * 8]), va):
        extra = ''
        for k in range(i.address, i.address + i.size):
            if k in fixmap:
                t = fixmap[k]; s = cstr(t); extra = f'   ; ->{t:x}' + (f' "{s[:60]}"' if s else '')
        print(f'{i.address:08x}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}{extra}')
        n -= 1
        if n <= 0: break
def xrefs(target):
    return [k for k, v in fixmap.items() if v == target]
if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'x':
        for k in xrefs(int(sys.argv[2], 16)): print(hex(k))
    else: dis(int(a, 16), int(sys.argv[2]) if len(sys.argv) > 2 else 40)
