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
gaps=[]
for i in range(len(rows)-1):
    e=rows[i][0]+rows[i][1]; g=rows[i+1][0]-e
    if g>=64: gaps.append((e,rows[i+1][0]))
with open('build/gaps.txt','w') as f:
    for a,b in gaps:
        d=subprocess.run([od,'-s','--start-address',hex(a),'--stop-address',hex(b),'build/melodee.elf'],capture_output=True,text=True).stdout
        f.write(f"=== {a:x}..{b:x} ({b-a} B)\n{d}\n")
PY
  ;;
test)
  python3 tools/ci_blob.py emit gaps build/gaps.txt --start 1 --max 8
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
