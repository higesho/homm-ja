# list functions (push ebp/mov ebp,esp after padding) in a range and count callers
import sys, struct
sys.argv=[sys.argv[0]]+sys.argv[1:]
exec(open(__file__.replace('funcs.py','hx.py')).read().split("if __name__")[0])
lo,hi=int(sys.argv[1],16),int(sys.argv[2],16)
ts=pe.sections[0]; o0=ts.PointerToRawData; v0=base+ts.VirtualAddress
calls={}
for o in range(o0, o0+ts.SizeOfRawData-5):
    if data[o]==0xe8:
        t=(v0+(o-o0)+5+struct.unpack_from('<i',data,o+1)[0])&0xffffffff
        if lo<=t<hi: calls.setdefault(t,[]).append(v0+o-o0)
for t in sorted(calls): print(hex(t), len(calls[t]), [hex(x) for x in calls[t][:6]])
