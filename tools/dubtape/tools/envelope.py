# dubdelay research: half-second RMS envelope (dBFS) of 16-bit WAVs, for a quick look at renders
import sys, wave, numpy as np
for f in sys.argv[1:]:
    w=wave.open(f); n=w.getnframes(); a=np.frombuffer(w.readframes(n),dtype='<i2').reshape(-1,2)/32768.
    hop=int(44100*0.5); rms=[20*np.log10(np.sqrt(np.mean(a[i:i+hop]**2))+1e-9) for i in range(0,n-hop+1,hop)]
    print(f.split('/')[-1][:28].ljust(28),' '.join('%4.0f'%r for r in rms))
