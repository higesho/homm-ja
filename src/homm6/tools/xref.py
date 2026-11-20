# find code references (imm32 / disp32) to file offsets or VAs in the exe
#   python tools/xref.py off:<hexfileoffset> | <hexVA> ...
import pefile, sys, struct, re
p = pefile.PE(r'L:/SteamLibrary/steamapps/common/Might and Magic Heroes VI/Might & Magic Heroes VI.exe', fast_load=True)
base = p.OPTIONAL_HEADER.ImageBase
text = p.sections[0]; t = text.get_data(); tva = base + text.VirtualAddress

def va(arg):
    if arg.startswith('off:'):
        return base + p.get_rva_from_offset(int(arg[4:], 16))
    return int(arg, 16)

for a in sys.argv[1:]:
    v = va(a); pat = struct.pack('<I', v)
    hits = [tva + m.start() for m in re.finditer(re.escape(pat), t)]
    print(a, hex(v), [hex(h) for h in hits[:20]])
