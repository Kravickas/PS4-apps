import sys, subprocess, numpy as np, math
sys.path.insert(0,'/tmp/post'); from dark_globe_test import setup, dirn, SR, Rp, tanh
from resolve_globe_ref import fog_colour
from PIL import Image
W,H=1920,1080; MPU=114.16
kR=np.array([1.24062e-6/0.68**4,1.24062e-6/0.55**4,1.24062e-6/0.44**4])*MPU; kM=5.328e-3/1200*MPU; HRr=8000/MPU; HMm=1200/MPU
def T_abs(mu):
    o=subprocess.run(['/tmp/hz/tabs','%.7f'%mu],capture_output=True,text=True).stdout.split(); return np.array([float(x) for x in o[1:4]])
cam=np.array([3.5,0.55,0.8]); yaw=0.3; pitch=-0.12
moon=dirn(14.0,math.degrees(yaw)+22.0); sun=-moon
F,R,U,d,desc,sd,md,sc,mc,tilt=setup(yaw,pitch,cam,sun)
hc=cam[1]+0.5+(cam[0]**2+cam[2]**2)/(2*Rp)
def tilted(L,t): tt=t[0]+(t[1]*L[0]+t[2]*L[2])/max(np.hypot(L[0],L[2]),1e-9); c=1/math.sqrt(1+tt*tt); return L[1]*c+np.hypot(L[0],L[2])*tt*c
ym=tilted(moon,tilt); Tm=T_abs(ym); T1=T_abs(1.0)
sc=np.zeros(3); mc=Tm*3.6375                                    # the sun is down; the moon through the air
d[:]=SR.consts(F,R,U,tanh,sun,np.pi,moon,0.1481,np.array([0,0,1.0]),tilt,(cam[0],cam[2],cam[1]+0.5,1/Rp))
md=(md[0],md[1],md[2],md[3])
def srgb2lin(c): c=np.asarray(c,float); return np.where(c<=0.04045,c/12.92,((c+0.055)/1.055)**2.4)
MOONCOL=srgb2lin([0.52,0.64,0.84]); ALB=np.array([0.147,0.149,0.155])
TEX=np.frombuffer(open('/tmp/hz/trans_exact.bin','rb').read(),np.float64).reshape(20001,3)
def Tratio(mu): x=np.clip(mu,0,1)*20000; i0=np.floor(x).astype(int); f=x-i0; i1=np.minimum(i0+1,20000); return TEX[i0]*(1-f)[:,None]+TEX[i1]*f[:,None]
def render(xs,ys):
    xa=(xs-960)/540; ya=(540-ys)/540
    V=F[None]+xa[:,None]*R[None]*tanh+ya[:,None]*U[None]*tanh; V/=np.linalg.norm(V,axis=1,keepdims=True)
    out=np.zeros((len(xs),3))
    a=(V[:,0]**2+V[:,2]**2)/(2*Rp); b=V[:,1]+(cam[0]*V[:,0]+cam[2]*V[:,2])/Rp; c=hc
    disc=b*b-4*a*c; flo=(disc>=0)&(b<0)
    t=np.where(flo,(-b-np.sqrt(np.maximum(disc,0)))/(2*np.maximum(a,1e-12)),0)
    sky=~flo
    if sky.any(): out[sky]=SR.reference(xa[sky],ya[sky],d,sd,md,sc,mc)
    if flo.any():
        Vf=V[flo]; tf=t[flo]; p=cam[None]+tf[:,None]*Vf
        n=np.stack([p[:,0]/Rp,np.ones(len(p)),p[:,2]/Rp],1); n/=np.linalg.norm(n,axis=1,keepdims=True)
        hlm=np.hypot(moon[0],moon[2]); hm=np.array([moon[0],moon[2]])/hlm
        tn=(p[:,0]*hm[0]+p[:,2]*hm[1])/Rp; cc=1/np.sqrt(1+tn*tn); ymp=moon[1]*cc+hlm*tn*cc
        lc=MOONCOL[None]*Tratio(np.abs(ymp))*(ymp>0)[:,None]
        e=0.070740275+0.929259717*np.maximum(0,(n@moon)*0.621)
        col=e[:,None]*ALB[None]*lc
        sh=np.array([sun[0],sun[2]])/np.hypot(sun[0],sun[2])
        C=fog_colour(Vf,(p[:,0],np.zeros(len(p)),p[:,2]),[(sun,sh,np.pi),(moon,-sh,0.1481)],tilt,1/Rp,hc)
        he=np.zeros(len(p)); Is=[]
        for Hs in (HRr,HMm):
            x=(he-hc)/Hs; E=np.exp(-hc/Hs)
            with np.errstate(all='ignore'): ex=tf*(E-np.exp(-he/Hs))/x
            Is.append(np.where(np.abs(x)<0.01,tf*E*(1-x/2+x*x/6),ex))
        Tr=np.exp(-(kR[None]*Is[0][:,None]+kM*Is[1][:,None]))
        out[flo]=C+(col-C)*Tr
    return out
ys,xs=np.mgrid[0:540,0:960]; lo=render(xs.ravel()*2+1.0,ys.ravel()*2+1.0).reshape(540,960,3)
img=np.stack([np.asarray(Image.fromarray(np.float32(lo[...,c]),'F').resize((W,H),Image.BILINEAR),np.float64) for c in range(3)],2)
# the moon at full resolution
mx=960+md[0]*540*(9/16)*16/9; mxp=960+md[0]*540; myp=540-md[1]*540
x0,x1=int(mxp-80),int(mxp+80); y0,y1=int(myp-80),int(myp+80)
yy,xx=np.mgrid[y0:y1,x0:x1]; patch=render(xx.ravel()+0.5,yy.ravel()+0.5).reshape(y1-y0,x1-x0,3); img[y0:y1,x0:x1]=patch
# stars (ps_stars: a separate pass) - random, above the horizon, faint
rng=np.random.default_rng(7)
for _ in range(700):
    sx=rng.integers(0,W); sy=rng.integers(0,H)
    if lo[min(sy//2,539),min(sx//2,959)].sum()<0.02 and sy<600: img[sy,sx]+=rng.uniform(0.03,0.35)
np.save('/tmp/clk/night_lin.npy',np.clip(img,0,1))
s=np.where(img<=0.0031308,12.92*img,1.055*np.clip(img,0,1)**(1/2.4)-0.055)
Image.fromarray(np.uint8(np.round(np.clip(s,0,1)*255))).save('/tmp/clk/night_scene.png')
print("moon on screen at (%.0f, %.0f), tilted y %.4f, disc colour %s, sky range %.4f..%.4f"%(mxp,myp,ym,np.round(mc,3),lo.min(),lo.max()))
