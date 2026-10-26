# dubdelay research: loop bodies (backward branches) and their static instruction counts in a pi32v2 disassembly
import re,sys
txt=open(sys.argv[1]).read().split('\n')
cur=None; fn={}
for l in txt:
    m=re.match(r'^([a-z_0-9]+):$',l)
    if m: cur=m.group(1); fn[cur]=[]; continue
    m=re.match(r'^ +([0-9a-f]+):\s',l)
    if m and cur: fn[cur].append((int(m.group(1),16),l))
for f in ['run_lite','run_tape','hb_dec','hb_int','control_tick']:
    ins=fn[f]; addrs=[a for a,_ in ins]
    for a,l in ins:
        m=re.search(r'goto -\d+ <[^+]+\+0x([0-9A-Fa-f]+)',l)
        if m:
            t=int(m.group(1),16)
            n=sum(1 for x in addrs if t<=x<=a)
            print(f, hex(t),'->',hex(a), n,'instr')
