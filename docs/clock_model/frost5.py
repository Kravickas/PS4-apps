exec(open('/tmp/clk/slab.py').read())
TEX=np.frombuffer(open('/tmp/hz/trans_exact.bin','rb').read(),np.float64).reshape(20001,3)
def Tr(mu): return TEX[int(round(np.clip(mu,0,1)*20000))]
PW,PH,RC=1152,648,56; d=rrect(CX,CY,PW/2,PH/2,RC)
def rim_points(step=3.0):
    """the panel's rounded-rect boundary: points and outward normals every `step` px"""
    hx,hy=PW/2-RC,PH/2-RC; pts=[]; nrm=[]
    for (cx,cy,a0) in ((hx,-hy,-90),(hx,hy,0),(-hx,hy,90),(-hx,-hy,180)):   # corner arcs (screen y down)
        n=int(math.ceil(math.pi/2*RC/step))
        for i in range(n):
            a=math.radians(a0+90*(i+0.5)/n); pts.append((CX+cx+RC*math.cos(a),CY+cy+RC*math.sin(a))); nrm.append((math.cos(a),math.sin(a)))
    for (x0,y0,x1,y1,nx,ny) in ((-hx,-PH/2,hx,-PH/2,0,-1),(PW/2,-hy,PW/2,hy,1,0),(hx,PH/2,-hx,PH/2,0,1),(-PW/2,hy,-PW/2,-hy,-1,0)):
        L=math.hypot(x1-x0,y1-y0); n=int(L/step)
        for i in range(n):
            t=(i+0.5)/n; pts.append((CX+x0+(x1-x0)*t,CY+y0+(y1-y0)*t)); nrm.append((nx,ny))
    return np.array(pts),np.array(nrm)
def internal_light(l2,Ls=170.0,s0=3.0,spread=0.07,sc=4):
    """light entering the slab's rim from direction l2 (2D, the way the light travels): refracted in
    (Snell, IOR 1.5; Schlick transmission), carried as beams that widen by scattering in the frost and
    fade over Ls. Returns the intensity field I and the mean travel direction (tx, ty), at 1/sc res."""
    P,N=rim_points(); cosi=-(N@l2); ok=cosi>0; P,N,cosi=P[ok],N[ok],cosi[ok]
    eta=1/1.5; k=1-eta*eta*(1-cosi**2); tdir=eta*l2[None,:]+(eta*cosi-np.sqrt(k))[:,None]*N
    tdir/=np.linalg.norm(tdir,axis=1,keepdims=True)
    Fr=0.04+0.96*(1-cosi)**5; w=cosi*(1-Fr)*3.0            # power per rim sample (3 px of rim)
    gy,gx=np.mgrid[0:H//sc,0:W//sc]; X=(gx.ravel()+0.5)*sc; Y=(gy.ravel()+0.5)*sc
    inside=rrect_sdf(0,0,0,0,0) if False else None
    I=np.zeros(X.size); TX=np.zeros(X.size); TY=np.zeros(X.size)
    for b in range(0,len(P),64):
        px=P[b:b+64,0][None]; py=P[b:b+64,1][None]; tx=tdir[b:b+64,0][None]; ty=tdir[b:b+64,1][None]; ww=w[b:b+64][None]
        vx=X[:,None]-px; vy=Y[:,None]-py; al=vx*tx+vy*ty; pe=np.abs(vx*ty-vy*tx)
        sig=s0+spread*np.maximum(al,0)
        c=np.where(al>0,ww*np.exp(-np.sqrt(vx*vx+vy*vy)/Ls)*np.exp(-0.5*(pe/sig)**2)/(sig*2.5066),0)
        I+=c.sum(1); TX+=(c*tx).sum(1); TY+=(c*ty).sum(1)
    I=I.reshape(H//sc,W//sc); TX=TX.reshape(I.shape); TY=TY.reshape(I.shape)
    up=lambda a: np.asarray(Image.fromarray(np.float32(a),'F').resize((W,H),Image.BILINEAR),np.float64)
    I=up(I); TX=up(TX); TY=up(TY); n=np.hypot(TX,TY)+1e-9
    return I,TX/n,TY/n
def clock5(scene,light_xy,light_col,strength,name,frost=45.0,fill=0.18):
    col=np.asarray(light_col,float); col=col/col.max()
    l2=np.array([CX-light_xy[0],CY-light_xy[1]]); l2/=np.linalg.norm(l2)
    img=slab_faceon(scene,d,bevel=40,frost=frost,dbg=300,bbox=(CX-PW/2,CY-PH/2,CX+PW/2,CY+PH/2))
    m=np.clip(0.5-d,0,1)
    grain=np.random.default_rng(3).normal(0,1,(H,W)); grain=nd.gaussian_filter(grain,0.6)*1.8
    img=img*(1+0.035*grain*m)[...,None]                    # the frost's grain
    I,tx,ty=internal_light(l2)
    ref=np.percentile(I[(m>0.5)&(d>-60)&(d<-30)],90)          # the light just inside the lit rim = 1
    I=I/ref
    img=img+(strength*fill*np.clip(I,0,2.0)*m)[...,None]*col            # light scattered out by the frost
    gy,gx=np.gradient(nd.gaussian_filter(d,1.2)); gl=np.hypot(gx,gy)+1e-6; nx,ny=gx/gl,gy/gl
    band=np.maximum(0,1-np.abs(d+3)/3)
    exitl=band*np.clip(I,0,1.5)*np.maximum(0,nx*tx+ny*ty)                # leaving at the far edges
    glint=band*np.maximum(0,-(nx*l2[0]+ny*l2[1]))**40                     # where it strikes the rim
    img=img+strength*((exitl*0.9+nd.gaussian_filter(exitl,5)*0.4)[...,None]*col+(glint*2.2+nd.gaussian_filter(glint,12)*1.8)[...,None]*(0.5+0.5*col))
    # glass text: windows (blur 2) with refracting bevels; the internal light catches the walls facing it
    L=Layer(); fb=L.font(DEMI,300); fs=L.font(DEMI,110); fd=L.font(DEMI,44)
    t='15:40'; wt=L.tw(t,fb); ws=L.tw(':26',fs); x0=CX-(wt+ws+8)/2; yb=CY+85
    dd=np.minimum.reduce([text_sdf(L,x0,yb,t,fb,anchor='ls'),text_sdf(L,x0+wt+8,yb,':26',fs,anchor='ls'),
                          text_sdf(L,CX,yb+95,'Monday 28 September 2026',fd,anchor='ms')])
    mi=np.clip(0.5-dd,0,1)
    win=slab_faceon(scene,dd,bevel=7.0,frost=2.0,dbg=140,shadow=False)
    img=img+mi[...,None]*(win-img)
    dark=np.clip(1-np.abs(dd-1.7)/1.3,0,1)*(1-mi); img=img*(1-0.35*dark)[...,None]
    gy2,gx2=np.gradient(nd.gaussian_filter(dd,1.0)); gl2=np.hypot(gx2,gy2)+1e-6
    face=np.maximum(0,-(gx2*tx+gy2*ty)/gl2)                              # wall facing the incoming light
    rim=np.maximum(0,1-np.abs(dd+1.3)/1.3)
    Ic=np.clip(I,0.15,1.5)
    img=img+strength*((rim*(0.25+1.1*face)*Ic+nd.gaussian_filter(rim*face*Ic,2.5)*mi*1.2)[...,None]*col)
    save(l2s(img),'/tmp/clk/%s.png'%name); print(name)
