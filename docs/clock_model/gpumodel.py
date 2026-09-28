# Per-pixel model of ps_clock (docs/clock_plan.txt): only what one pixel can compute - analytic slab
# SDF / bevel, 3 frost samples, the internal light texture (I, T), 7 text-SDF samples.
exec(open('/tmp/clk/frost5.py').read())
IORS=np.array([1.514,1.517,1.522]); ABSC=np.array([0.0010,0.0005,0.0008])
def bilin(img,x,y):
    return np.stack([nd.map_coordinates(img[...,c],[y-0.5,x-0.5],order=1,mode='nearest') for c in range(img.shape[2])],-1)
def rr_sdf_grad(px,py,cx,cy,hw,hh,r):
    qx=np.abs(px-cx)-hw+r; qy=np.abs(py-cy)-hh+r
    mx=np.maximum(qx,0); my=np.maximum(qy,0); L=np.hypot(mx,my)
    d=L+np.minimum(np.maximum(qx,qy),0)-r
    sx=np.sign(px-cx); sy=np.sign(py-cy)
    gx=np.where(L>0,mx/np.maximum(L,1e-9),(qx>qy).astype(float))*sx; gy=np.where(L>0,my/np.maximum(L,1e-9),(qy>=qx).astype(float))*sy
    return d,gx,gy
def bevel_normal(s,bev,gx,gy):
    u=np.clip(s/bev,0.02,1.0); slope=np.where(s<bev,(1-u)/np.sqrt(1-(1-u)**2),0.0)
    nx,ny=slope*gx,slope*gy; l=np.sqrt(nx*nx+ny*ny+1); return nx/l,ny/l,1/l,bev*np.sqrt(np.clip(1-(1-u)**2,0,1))
def grain_hash(X,Y):
    h=(X.astype(np.uint64)*np.uint64(73856093)^Y.astype(np.uint64)*np.uint64(19349663))&np.uint64(0xffffffff)
    h=(h^(h>>np.uint64(13)))*np.uint64(1274126177)&np.uint64(0xffffffff); h=h^(h>>np.uint64(16))
    return (h.astype(np.float64)/4294967295.0*2-1)*1.7320508   # uniform, unit variance
def clock_gpu(scene,frost_tex_up,Ifield,tx,ty,light_xy,col,strength,fill,text_d,text_g):
    col=np.asarray(col,float); col=col/col.max()
    l2=np.array([CX-light_xy[0],CY-light_xy[1]]); l2/=np.linalg.norm(l2)
    X=XX; Y=YY
    d,gx,gy=rr_sdf_grad(X,Y,CX,CY,PW/2,PH/2,RC); s=np.maximum(-d,0); m=np.clip(0.5-d,0,1)
    nx,ny,nz,h=bevel_normal(s,40.0,gx,gy)
    out=np.zeros_like(scene)
    for c in range(3):
        rx,ry,rz,tir=refract(0,0,-1,nx,ny,nz,1/IORS[c]); Lp=h/np.maximum(np.abs(rz),1e-3)
        ex,ey=rx*IORS[c],ry*IORS[c]; e2=ex*ex+ey*ey; tir2=e2>=1; ez=np.sqrt(np.maximum(1-e2,1e-4))
        ox=rx*Lp+ex*300/ez; oy=ry*Lp+ey*300/ez
        smp=bilin(frost_tex_up[...,c:c+1],X+ox,Y+oy)[...,0]
        out[...,c]=np.where(tir|tir2,frost_tex_up[...,c]*0.35,smp)*np.exp(-ABSC[c]*Lp)
    F=0.04+0.96*(1-nz)**5; kr=np.maximum(0,-(nx*-0.45+ny*-0.89)); env=0.10+1.6*kr**6
    table=out*(1-F[...,None])+(F*env)[...,None]
    I=np.clip(Ifield,0,2.0)
    table=table+(strength*fill*I*m)[...,None]*col
    band=np.maximum(0,1-np.abs(d+3)/3); wide=np.exp(-((d+3)/7.0)**2)
    ndT=np.maximum(0,gx*tx+gy*ty); ndl=np.maximum(0,-(gx*l2[0]+gy*l2[1]))
    table=table+strength*(((band*0.9+wide*0.4)*np.clip(Ifield,0,1.5)*ndT)[...,None]*col+((band*2.2*ndl**40+np.exp(-((d+3)/16.0)**2)*1.2*ndl**20))[...,None]*(0.5+0.5*col))
    nograin=table.copy()
    table=table*(1+0.035*grain_hash(X.astype(int),Y.astype(int))*m)[...,None]
    # the slab's shadow outside (the bevel band shifted 16 down / 8 right, soft) and its faint caustic
    ds,_,_=rr_sdf_grad(X-8,Y-16,CX,CY,PW/2,PH/2,RC)
    sh=np.exp(-((ds+14)/24.0)**2)*(ds>-60); ca=np.exp(-((ds+36)/9.0)**2)*0.6
    res=scene*(1-0.55*0.55*sh*(1-m))[...,None]+(0.10*ca*(1-m))[...,None]*np.array([1.0,0.93,0.8])
    res=res+m[...,None]*(table-res)
    # text on the table (in slab coords)
    ox0,oy0=CX-PW/2,CY-PH/2
    TH=np.array([0,1.0,0.55,0.33])
    def tsamp(dx,dy):
        u=X-ox0+dx; v=Y-oy0+dy
        dd=nd.map_coordinates(text_d,[v-0.5,u-0.5],order=1,mode='constant',cval=16.0)
        gg=nd.map_coordinates(text_g.astype(float),[np.round(v-0.5),np.round(u-0.5)],order=0,mode='constant',cval=0).astype(int)
        return dd,gg
    dt,gt=tsamp(0,0)
    shade=np.zeros_like(dt); caus=np.zeros_like(dt)
    for g in (1,2,3):
        th=TH[g]; off=7.0*th
        dsh,gsh=tsamp(-l2[0]*off,-l2[1]*off); sg=1.5+3.0*th
        shade=np.maximum(shade,np.where(gsh==g,np.clip(0.5-dsh/(2.5*sg),0,1),0))
        dcs,gcs=tsamp(-l2[0]*off*1.3,-l2[1]*off*1.3); wr=3.0*th+1.0+2.5*th
        caus=caus+np.where(gcs==g,np.exp(-((dcs+3.0*th)/wr)**2)*(dcs<0),0)
    mi=np.clip(0.5-dt,0,1)
    res=res*(1-0.40*shade*(1-mi))[...,None]+(strength*0.30*caus*(1-mi))[...,None]*col
    contact=np.clip(1-np.abs(dt-1.2)/1.2,0,1)*(1-mi); res=res*(1-0.40*contact)[...,None]
    # letters: bevel 9 from the text SDF gradient (2 more samples), the table under them (no grain),
    # TIR darkening, Fresnel, the light escaping the table into them, lit rims
    dxp,_=tsamp(1,0); dyp,_=tsamp(0,1); lgx=dxp-dt; lgy=dyp-dt; ll=np.hypot(lgx,lgy)+1e-6; lgx/=ll; lgy/=ll
    lnx,lny,lnz,lh=bevel_normal(np.maximum(-dt,0),9.0,lgx,lgy)
    let=np.zeros_like(scene)
    for c in range(3):
        rx,ry,rz,tir=refract(0,0,-1,lnx,lny,lnz,1/IORS[c])
        let[...,c]=np.where(tir,nograin[...,c]*0.35,nograin[...,c])*np.exp(-ABSC[c]*lh/np.maximum(np.abs(rz),1e-3))
    LF=0.04+0.96*(1-lnz)**5; lkr=np.maximum(0,-(lnx*-0.45+lny*-0.89))
    let=let*(1-LF[...,None])+(LF*(0.10+1.6*lkr**6))[...,None]
    let=let+(strength*0.55*np.clip(Ifield,0.35,1.6))[...,None]*col
    lb=np.maximum(0,1-np.abs(dt+1.5)/1.5)
    lg=lb*np.maximum(0,-(lgx*l2[0]+lgy*l2[1]))**6; le=lb*np.maximum(0,lgx*l2[0]+lgy*l2[1])
    let=let+strength*((lg*1.1+le*0.55)[...,None]*col+(np.exp(-((dt+1.5)/3.0)**2)*np.maximum(0,-(lgx*l2[0]+lgy*l2[1]))**6*0.5)[...,None]*col)
    res=res+(mi*m)[...,None]*(let-res)
    return res
