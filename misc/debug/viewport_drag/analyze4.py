# Does the tracking error grow over a long drag? Split the drag into 4 time slices.
import re, sys, statistics as st
HOST_R = 80+1280
def rows(path):
    out=[]
    for line in open(path):
        if not line.startswith('F') or 'dt=' not in line: continue
        g=dict(re.findall(r'(\w+)=([\d.,x-]+)', line))
        def pair(k):
            v=g.get(k,'-1,-1').split(',')[:2]; return float(v[0]), float(v[1])
        out.append(dict(dt=float(g['dt']), truth=pair('truth'), os=pair('os'),
                        owned=int(g['owned']), moving=('moving=-' not in line)))
    return out
for path in sys.argv[1:]:
    rs=[r for r in rows(path) if r['moving'] and r['owned'] and r['os'][0]>=0]
    print(f"== {path}: {len(rs)} frames on its own OS window while dragging")
    if len(rs) < 40: print("   too few\n"); continue
    q = len(rs)//4
    for i in range(4):
        b = rs[i*q:(i+1)*q] if i<3 else rs[3*q:]
        off=[r['os'][0]-r['truth'][0] for r in b]
        med=st.median(off); dev=sorted(abs(o-med) for o in off); n=len(dev)
        flips=sum(1 for a,c in zip(off,off[1:]) if a!=c)
        dts=sorted(r['dt'] for r in b)
        print(f"   quarter {i+1}: median offset {med:7.1f} px | jitter p90 {dev[int(.9*n)]:3.0f} max {dev[-1]:3.0f}"
              f" | changes {100*flips/max(1,n-1):4.0f}% | dt med {st.median(dts):4.1f} max {dts[-1]:6.1f}")
    print()
