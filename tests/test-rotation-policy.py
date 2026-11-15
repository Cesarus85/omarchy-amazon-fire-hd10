import importlib.machinery,importlib.util,math
loader=importlib.machinery.SourceFileLoader('rotation',str(__import__('pathlib').Path(__file__).resolve().parents[1]/'platform/payload/usr/local/bin/trona-auto-rotate'))
spec=importlib.util.spec_from_loader(loader.name,loader);r=importlib.util.module_from_spec(spec);loader.exec_module(r)
# Recorded physical poses, transformed using the original Trona mounting matrix.
assert r.classify((-7*.009577,898*.009577,455*.009577))==0
assert r.classify((-1008*.009577,-8*.009577,230*.009577))==1
# Boundary/noise: lying flat, diagonal grip and acceleration must retain orientation.
for pose in [(0,0,9.81),(6.9,6.9,0),(0,15,0),(0,0,0)]:assert r.classify(pose) is None,pose
# Quick alternating grips and finger contact must never commit a rotation.
s=r.Stability()
for i in range(20):assert s.update(i%2,i*.2) is None
assert s.update(1,5,True) is None
assert s.update(1,5.2) is None
assert s.update(1,6.1)==1
s.applied(6.1)
assert s.update(0,6.2) is None
assert s.update(0,7.1) is None
assert s.update(0,8.2)==0
assert s.update(None,8.3) is None
print('PASS: measured sensor axes, flat/diagonal/shake rejection, contact guard, debounce and cooldown')
