# heroes4.exe helpers: memory image, xrefs by absolute address, disassembly
import os, sys, struct, re, capstone, pefile
EXE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'heroes4.exe')
P = pefile.PE(EXE); M = P.get_memory_mapped_image(); BASE = 0x400000
TEXT = (0x401000, 0x401000 + 0x56e30a)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def cstr(va):
    o = va - BASE
    if not (0 <= o < len(M)): return None
    e = M.find(b'\0', o); s = M[o:e]
    return s.decode('latin-1') if 0 < len(s) < 200 and all(32 <= c < 127 or c in (9, 10, 13) for c in s) else None
def xrefs(va):
    b = struct.pack('<I', va); out = []; o = TEXT[0] - BASE
    while True:
        o = M.find(b, o, TEXT[1] - BASE)
        if o < 0: break
        out.append(BASE + o); o += 1
    return out
def dis(va, n=40, out=None):
    o = va - BASE; res = []
    for i in md.disasm(bytes(M[o:o + n * 10]), va):
        extra = ''
        for m in re.findall(r'0x[0-9a-f]{6,}', i.op_str):
            s = cstr(int(m, 16))
            if s: extra = '   ; "%s"' % s[:60]
        line = '%08x  %-20s %s %s%s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str, extra)
        res.append(line)
        n -= 1
        if n <= 0: break
    if out is None: print('\n'.join(res))
    return res
if __name__ == '__main__':
    a = sys.argv[1]
    if a == 'x':
        for x in xrefs(int(sys.argv[2], 16)): print(hex(x))
    else: dis(int(a, 16), int(sys.argv[2]) if len(sys.argv) > 2 else 40)
