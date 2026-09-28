import sys, numpy as np, struct, math; sys.path.insert(0,'/tmp/post'); from gcnemu4 import Emu4, f32
exec(open('/tmp/clk/light_entries.py').read())
from PIL import Image
fb=lambda x: struct.unpack('<I',struct.pack('<f',float(x)))[0]
def s2l_(c): return np.where(c<=0.04045,c/12.92,((c+0.055)/1.055)**2.4)
frame=s2l_(np.asarray(Image.open('/tmp/sun1642/20260928_154026_00002526.jpg').convert('RGB'),np.float64)/255)
# the heavy frost as the GPU gets it: 60 x 34 (box-average of a sigma-45 blur, stand-in for the chain)
import scipy.ndimage as nd
fb45=np.stack([nd.gaussian_filter(frame[...,c],45,mode='nearest') for c in range(3)],2)
frost=np.stack([np.asarray(Image.fromarray(np.float32(fb45[...,c]),'F').resize((60,34),Image.BILINEAR),np.float64) for c in range(3)],2)
T=np.frombuffer(open('/tmp/tst/clock_tex.bin','rb').read(),np.uint8).reshape(648,1152,2)
sun=(800.0,380.0); l2=np.array([960-sun[0],540-sun[1]]); l2/=np.linalg.norm(l2)
E=entries(l2)
gy,gx=np.mgrid[0:270,0:480]; I,TX,TY=field_ref(E,(gx.ravel()+0.5)*4,(gy.ravel()+0.5)*4)
light=np.stack([I.reshape(270,480),TX.reshape(270,480),TY.reshape(270,480)],2)
def bil(tex,u,v):   # GPU bilinear, clamp to edge, normalized coords
    Hh,Ww=tex.shape[:2]; x=u*Ww-0.5; y=v*Hh-0.5; x0=np.floor(x).astype(int); y0=np.floor(y).astype(int); fx=x-x0; fy=y-y0
    x1=np.clip(x0+1,0,Ww-1); y1=np.clip(y0+1,0,Hh-1); x0=np.clip(x0,0,Ww-1); y0=np.clip(y0,0,Hh-1)
    t=tex if tex.ndim==3 else tex[...,None]
    r=(t[y0,x0]*(1-fx)[:,None]+t[y0,x1]*fx[:,None])*(1-fy)[:,None]+(t[y1,x0]*(1-fx)[:,None]+t[y1,x1]*fx[:,None])*fy[:,None]
    return r
def samp(off,u,v):
    u=np.asarray(u,np.float64); v=np.asarray(v,np.float64)
    r={0:lambda: bil(frame,u,v),12:lambda: bil(frost,u,v),20:lambda: bil(light,u,v),28:lambda: bil(T[...,0].astype(np.float64)/255.0,u,v)}[off]()
    return [r[:,k].astype(f32) for k in range(r.shape[1])]
def load(off,xi,yi,si):
    assert off==28; xi=np.clip(np.asarray(xi).astype(np.int64),0,1151); yi=np.clip(np.asarray(yi).astype(np.int64),0,647)
    t=T[yi,xi].astype(np.float64)/255.0; return [t[:,0].astype(f32),t[:,1].astype(f32)]
col=np.array([1.0,0.62,0.35]); strength,fill=1.35,0.18
tab=np.zeros(64,np.uint32)
for i,v in enumerate([960,540,576,324,56,1/1920,1/1080,strength,col[0],col[1],col[2],fill,l2[0],l2[1],384,216,1/1152,1/648]): tab[40+i]=fb(v)
