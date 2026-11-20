# map addresses from the GOG Complete Heroes3.exe to another SoD-based exe by masked signature search
import sys, re, capstone, struct
A = open(r'L:/Games/GOG/TocaRaceDriver3/HoMM 3 Complete/Heroes3.exe', 'rb').read()
B = open(sys.argv[1], 'rb').read()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = False
def sig(va, n=48):
    o = va - 0x400000; pat = b''; cnt = 0
    for i in md.disasm(A[o:o + 200], va):
        b = A[i.address - 0x400000:i.address - 0x400000 + i.size]
        m = re.escape(b)
        if i.mnemonic in ('call', 'jmp') and b[0] in (0xe8, 0xe9): m = re.escape(b[:1]) + b'....'
        elif b[0] in (0x0f,) and len(b) == 6 and 0x80 <= b[1] <= 0x8f: m = re.escape(b[:2]) + b'....'
        else:
            # mask 4-byte absolute addresses inside the image
            bb = bytearray(b); mm = b''
            k = 0
            while k < len(bb):
                if k + 4 <= len(bb) and 0x400000 <= struct.unpack_from('<I', bb, k)[0] < 0x700000:
                    mm += b'....'; k += 4
                else:
                    mm += re.escape(bytes([bb[k]])); k += 1
            m = mm
        pat += m; cnt += len(b)
        if cnt >= n: break
    return pat
res = {}
for a in sys.argv[2:]:
    va = int(a, 16)
    hits = [m.start() + 0x400000 for m in re.finditer(sig(va), B, re.S)]
    print(a, [hex(h) for h in hits[:4]], len(hits))
