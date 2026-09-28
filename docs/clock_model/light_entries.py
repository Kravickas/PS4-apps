import numpy as np, math, re
exec(open('/tmp/clk/frost5.py').read().split("def internal_light")[0])   # rim_points + constants
NORM=[float(x) for x in re.findall(r"([\d.]+)f,",open('/home/claude/w/shadcube4_build/src/clock_light_norm.h').read())]
def norm_of(l2):
    a=math.degrees(math.atan2(l2[1],l2[0]))%360.0; x=a/2.0; i0=int(x)%180; f=x-int(x); return NORM[i0]*(1-f)+NORM[(i0+1)%180]*f
def entries(l2):
    """the CPU's list: lit rim samples (every 3 px), refracted direction, weight cos (1-F) 3 / (sqrt(2 pi) norm)"""
    P,N=rim_points(3.0); cosi=-(N@l2); ok=cosi>0; P,N,cosi=P[ok],N[ok],cosi[ok]
    eta=1/1.5; k=1-eta*eta*(1-cosi**2); t=eta*l2[None,:]+(eta*cosi-np.sqrt(k))[:,None]*N; t/=np.linalg.norm(t,axis=1,keepdims=True)
    w=cosi*(1-(0.04+0.96*(1-cosi)**5))*3.0/(2.5066282746310002*norm_of(l2))
    E=np.zeros((len(P),8)); E[:,0:2]=P; E[:,2:4]=t; E[:,4]=w; return E
def field_ref(E,X,Y,Ls=170.0,s0=3.0,spread=0.07):
    I=np.zeros(len(X)); TX=np.zeros(len(X)); TY=np.zeros(len(X))
    for e in E:
        vx=X-e[0]; vy=Y-e[1]; al=vx*e[2]+vy*e[3]; pe=vx*e[3]-vy*e[2]; sig=s0+spread*np.maximum(al,0)
        c=np.where(al>0,e[4]*np.exp(-np.sqrt(vx*vx+vy*vy)/Ls)*np.exp(-0.5*(pe/sig)**2)/sig,0); I+=c; TX+=c*e[2]; TY+=c*e[3]
    return I,TX,TY
