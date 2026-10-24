# Split drag frames by whether the dragged window still OVERLAPS the host OS window.
# Host OS window: pos 80,60 size 1280x800 -> right edge 1360. Drag is horizontal.
import re, sys, statistics as st
HOST_R = 80+1280
def rows(path):
    out=[]
    for line in open(path):
        if not line.startswith('F') or 'dt=' not in line: continue
        g=dict(re.findall(r'(\w+)=([\d.,x-]+)', line))
        def pair(k):
            v=g.get(k,'-1,-1').split(',')[:2]; return float(v[0]), float(v[1])
        out.append(dict(dt=float(g['dt']), truth=pair('truth'), pos=pair('pos'), os=pair('os'),
                        owned=int(g['owned']), contained=int(g.get('contained',-1)),
                        moving=('moving=-' not in line)))
    return out
def stats(rs, label, key):
    rs=[r for r in rs if r[key][0]>=0]
    if len(rs)<5: print(f"   {label:34s} {len(rs):4d} frames (too few)"); return
    off=[r[key][0]-r['truth'][0] for r in rs]
    med=st.median(off); dev=sorted(abs(o-med) for o in off); n=len(dev)
    dts=sorted(r['dt'] for r in rs)
    # how often the offset changes from the previous frame = what the eye sees as wobble
    flips=sum(1 for a,b in zip(off,off[1:]) if a!=b)
    print(f"   {label:34s} {n:4d} fr | jitter med {st.median(dev):4.0f} p90 {dev[int(.9*n)]:4.0f} max {dev[-1]:4.0f} px"
          f" | changes {100*flips/max(1,n-1):4.0f}% of frames | dt med {st.median(dts):4.1f} max {dts[-1]:6.1f}")
for path in sys.argv[1:]:
    rs=[r for r in rows(path) if r['moving']]
    print(f"== {path}  ({len(rs)} drag frames)")
    stats([r for r in rs if not r['owned']],                      "merged, over host (imgui pos)", 'pos')
    stats([r for r in rs if r['owned'] and r['os'][0] <  HOST_R], "own OS win, OVER host  (real)", 'os')
    stats([r for r in rs if r['owned'] and r['os'][0] >= HOST_R], "own OS win, off host   (real)", 'os')
    print()
