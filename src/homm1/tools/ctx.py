# code context of every string reference: the call that follows it (to tell display text from internal strings)
import os, sys, json, pickle, bisect, collections
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import strings as S
L = S.L
ins = pickle.load(open(os.path.join(ROOT, 'work', 'ins.pkl'), 'rb'))
addrs = [a for a, s, m, o in ins]
B1 = L.objs[0]['base']; B2 = L.objs[1]['base']

def ctx(site_va, n=8):
    i = bisect.bisect_right(addrs, site_va) - 1
    out = []
    for a, s, m, o in ins[i:i + n]:
        out.append((a, m, o))
    return out

def next_call(site_va):
    for a, m, o in ctx(site_va, 10)[1:]:
        if m == 'call': return o
        if m in ('ret', 'jmp'): return None
    return None

if __name__ == '__main__':
    R = json.load(open(os.path.join(ROOT, 'work', 'strings.json')))
    calls = collections.Counter()
    info = {}
    for r in R:
        cs = []
        for oi, so in S.refs.get(r['off'], []):
            if oi == 0:
                c = next_call(B1 + so); cs.append(c or '-')
            else: cs.append('data')
        info[r['off']] = cs
        for c in cs: calls[c] += 1
    json.dump({hex(k): v for k, v in info.items()}, open(os.path.join(ROOT, 'work', 'ctx.json'), 'w'))
    for c, n in calls.most_common(40):
        ex = [r['s'][:40] for r in R if c in info[r['off']]][:4]
        print(c, n, ex)
