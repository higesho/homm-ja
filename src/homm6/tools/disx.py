import pefile,capstone,sys
p=pefile.PE(r'L:/SteamLibrary/steamapps/common/Might and Magic Heroes VI/Might & Magic Heroes VI.exe',fast_load=True)
s=p.sections[0]; t=s.get_data(); base=p.OPTIONAL_HEADER.ImageBase+s.VirtualAddress
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
a=int(sys.argv[1],16); n=int(sys.argv[2],16) if len(sys.argv)>2 else 0x100
for ins in md.disasm(t[a-base:a-base+n],a):
    print(hex(ins.address),ins.mnemonic,ins.op_str)
    if ins.mnemonic=='ret' and len(sys.argv)<4: print('---')
