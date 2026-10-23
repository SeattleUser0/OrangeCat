# (size audit, temporary) extra probes of the target build, as notices (tools/ci_blob.py). $1: build | test
set -u
OD="$JIELI_TOOLCHAIN/common/bin/objdump"
case "$1" in
build)
  # the anonymous data in .text (string literals, jump tables, unnamed constants): the ranges between named symbols
  python3 - "$OD" <<'PY'
import re,subprocess,sys
od=sys.argv[1]
syms=subprocess.run([od,'-t','build/melodee.elf'],capture_output=True,text=True).stdout
line=re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+([0-9a-f]{8})\s+(\S+)\s*$")
rows=sorted((int(m[1],16),int(m[4],16),m[5]) for m in map(line.match,syms.splitlines()) if m and m[3]=='.text' and int(m[4],16))
img=open('build/text.bin','rb').read(); base=0x02001000
with open('build/gaps.bin','wb') as f:
    for i in range(len(rows)-1):
        e=rows[i][0]+rows[i][1]; g=rows[i+1][0]-e
        if g>=64:
            f.write(f"=== {e:x}..{rows[i+1][0]:x} ({g} B) after {rows[i][2]}\n".encode()); f.write(img[e-base:rows[i+1][0]-base]); f.write(b"\n")
PY
  ;;
test)
  python3 tools/ci_blob.py emit gaps build/gaps.bin --start 1 --max 1
  ;;
esac
# ---- (probe 2) byte-identical constant objects in flash: every sized data symbol of .text hashed, duplicates listed
case "$1" in
test)
  python3 - "$OD" <<'PY'
import re,subprocess,sys,hashlib,collections
od=sys.argv[1]
syms=subprocess.run([od,'-t','build/melodee.elf'],capture_output=True,text=True).stdout
line=re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+([0-9a-f]{8})\s+(\S+)\s*$")
rows=[(int(m[1],16),int(m[4],16),m[5]) for m in map(line.match,syms.splitlines()) if m and m[3]=='.text' and int(m[4],16)>=16 and 'F' not in m[2]]
img=open('build/text.bin','rb').read(); base=0x02001000
by=collections.defaultdict(list)
for a,z,n in rows:
    by[hashlib.sha1(img[a-base:a-base+z]).hexdigest()].append((z,n))
with open('build/dups.txt','w') as f:
    for h,l in sorted(by.items(),key=lambda kv:-kv[1][0][0]*(len(kv[1])-1)):
        if len(l)>1: f.write(f"{l[0][0]:6} x{len(l)} {' '.join(n for z,n in l)}\n")
PY
  python3 tools/ci_blob.py emit dups build/dups.txt --start 1 --max 1
  ;;
esac
# ---- (probe 3) stack frames: each function's prologue lines that move sp (build/melodee.dis), and the call graph of
# the final IR (build/melodee_audio.ll), for a static worst-case stack bound
case "$1" in
test)
  python3 - <<'PY'
import re
dis=open('build/melodee.dis').read().splitlines()
out=[]; cur=None; n=0
for ln in dis:
    m=re.match(r'^([\w.]+):$',ln)
    if m: cur=m[1]; n=0; continue
    if cur is None: continue
    if re.match(r'^\s*[0-9a-f]+:',ln):
        n+=1
        if n<=10 and re.search(r'\bsp\b',ln): out.append(f"{cur}\t{ln.strip()}")
open('build/frames.txt','w').write("\n".join(out)+"\n")
ir=open('build/melodee_audio.ll').read()
edges=set(); cur=None
for ln in ir.splitlines():
    m=re.match(r'^define [^\n]*?@([\w.]+)\(',ln)
    if m: cur=m[1]; continue
    if ln.startswith('}'): cur=None; continue
    if cur:
        for c in re.findall(r'\bcall\b[^\n@]*@([\w.]+)\(',ln):
            if not c.startswith('llvm.'): edges.add((cur,c))
open('build/calls.txt','w').write("\n".join(f"{a} {b}" for a,b in sorted(edges))+"\n")
PY
  [ -f tools/ci_probe.conf ] && . tools/ci_probe.conf           # FRAMES_START / CALLS_START: the next chunks
  python3 tools/ci_blob.py emit frames build/frames.txt --start ${FRAMES_START:-1} --max ${FRAMES_MAX:-3}
  python3 tools/ci_blob.py emit calls build/calls.txt --start ${CALLS_START:-1} --max ${CALLS_MAX:-4}
  ;;
esac
