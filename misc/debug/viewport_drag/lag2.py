import re, sys, statistics as st
for path in sys.argv[1:]:
    rows=[]
    for line in open(path):
        m=re.match(r'\[drag\] f=(\d+) .*lag=(-?\d+),(-?\d+) dt=([\d.]+)', line)
        if m: rows.append((int(m.group(1)), int(m.group(2)), float(m.group(4))))
    if not rows: print(f"{path}: no drag frames"); continue
    # split into separate gestures on frame-number gaps
    blocks=[[rows[0]]]
    for r in rows[1:]:
        (blocks[-1] if r[0]-blocks[-1][-1][0] <= 20 else blocks.append([]) or blocks[-1]).append(r)
    b=max(blocks, key=len)   # the longest gesture = the measured oscillation
    lag=[r[1] for r in b]; dts=sorted(r[2] for r in b)
    med=st.median(lag); dev=sorted(abs(l-med) for l in lag); n=len(dev)
    flips=sum(1 for a,c in zip(lag,lag[1:]) if a!=c)
    print(f"{path}: {n} frames in the measured drag")
    print(f"   lag behind mouse: median {med:5.0f} px   spread p90 {dev[int(.9*n)]:3.0f}  max {dev[-1]:3.0f}")
    print(f"   lag CHANGES on {100*flips/max(1,n-1):4.0f}% of frames   (this is what reads as wobble)")
    print(f"   frame time: median {st.median(dts):4.1f} ms  max {dts[-1]:5.1f} ms")
