import pickle, sys, bisect
ins = pickle.load(open(__file__.replace('tools', 'work').replace('callers.py', 'ins.pkl'), 'rb'))
addrs = [i[0] for i in ins]
def callers(t):
    s = hex(t)
    return [a for a, l, m, o in ins if m == 'call' and o == s]
def func_start(a):
    # walk back to the previous int3/nop padding
    k = bisect.bisect_left(addrs, a)
    while k > 0 and ins[k - 1][2] not in ('int3', 'nop') and not (ins[k-1][2] in ('ret', 'jmp') and ins[k][0] % 16 == 0): k -= 1
    return ins[k][0]
if __name__ == '__main__':
    for t in sys.argv[1:]:
        t = int(t, 16)
        print(hex(t), ' '.join('%x(f%x)' % (c, func_start(c)) for c in callers(t)))
