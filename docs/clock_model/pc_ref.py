# float64 reference of ps_clock (the per-pixel model clock_gpu with GPU sampling), for pixel arrays X, Y
def pc_reference(X,Y):
    IORS=(1.514,1.517,1.522); ABSC=(0.0010,0.0005,0.0008); n=len(X)
    uv=lambda x,y: (x/1920.0,y/1080.0)
    fr=bil(frame,*uv(X,Y)); lf=bil(light,*uv(X,Y)); fq=bil(frost,*uv(X,Y))
    def rrect_(x,y,need_g):
        qx=np.abs(x-960)-576+56; qy=np.abs(y-540)-324+56; mx=np.maximum(qx,0); my=np.maximum(qy,0); L=np.hypot(mx,my)
        d=L+np.minimum(np.maximum(qx,qy),0)-56
        if not need_g: return d
        sx=np.where(x-960<0,-1.0,1.0); sy=np.where(y-540<0,-1.0,1.0)
        gx=np.where(L>0,mx/np.where(L>0,L,1),(qx>qy).astype(float))*sx; gy=np.where(L>0,my/np.where(L>0,L,1),(qx<=qy).astype(float))*sy
        return d,gx,gy
    def bev(s,b,gx,gy):
        u=np.clip(s/b,0.02,1.0); t=np.sqrt(np.maximum(1-(1-u)**2,0)); h=b*t; sl=np.where(s<b,(1-u)/t,0.0)
        nx=sl*gx; ny=sl*gy; nz=1/np.sqrt(nx*nx+ny*ny+1); return nx*nz,ny*nz,nz,h
    def refr(nx,ny,nz,h,eta):
        k=1-eta*eta*(1-nz*nz); tir=k<0; f=eta*nz-np.sqrt(np.maximum(k,0)); rx=f*nx; ry=f*ny; rz=np.maximum(np.abs(f*nz-eta),1e-3); return rx,ry,h/rz,tir
    d,gx,gy=rrect_(X,Y,True); m=np.clip(0.5-d,0,1); s=np.maximum(-d,0); nx,ny,nz,h=bev(s,40.0,gx,gy)
    tab=np.zeros((n,3))
    for c in range(3):
        rx,ry,lp,tir=refr(nx,ny,nz,h,1/IORS[c]); ex=IORS[c]*rx; ey=IORS[c]*ry; e2=ex*ex+ey*ey; tir=tir|(e2>=1)
        ez=300/np.sqrt(np.maximum(1-e2,1e-4)); sx_=rx*lp+ex*ez+X; sy_=ry*lp+ey*ez+Y
        smp=bil(frost,*uv(sx_,sy_))[:,c]; tab[:,c]=np.where(tir,0.35*fq[:,c],smp)*np.exp(-ABSC[c]*lp)
    F=0.04+0.96*(1-nz)**5; kr=np.maximum(0,0.45*nx+0.89*ny); env=0.10+1.6*kr**6
    tab=tab*(1-F)[:,None]+(F*env)[:,None]
    I=lf[:,0]; tl=np.sqrt(lf[:,1]**2+lf[:,2]**2+1e-18); tx=lf[:,1]/tl; ty=lf[:,2]/tl
    tab+= (strength*fill*np.clip(I,0,2)*m)[:,None]*col
    b3=np.maximum(0,1-np.abs(d+3)/3); wd=np.exp(-((d+3)/7)**2); ndT=np.maximum(0,gx*tx+gy*ty); ndl=np.maximum(0,-(gx*l2[0]+gy*l2[1]))
    ex_=strength*(b3*0.9+wd*0.4)*np.clip(I,0,1.5)*ndT; gl=strength*(2.2*b3*ndl**40+1.2*np.exp(-((d+3)/16)**2)*ndl**20)
    tab+= ex_[:,None]*col+gl[:,None]*(0.5+0.5*col)
    ng=tab.copy()
    xi=np.floor(X).astype(np.uint64); yi=np.floor(Y).astype(np.uint64)
    hsh=((xi*np.uint64(0x046f4f3d))&np.uint64(0xffffffff))^((yi*np.uint64(0x0127409f))&np.uint64(0xffffffff))
    hsh=hsh^(hsh>>np.uint64(13)); hsh=(hsh*np.uint64(0x4bf14f21))&np.uint64(0xffffffff); hsh=hsh^(hsh>>np.uint64(16))
    gr=(hsh.astype(np.float64)*(2/4294967295.0)-1)*1.7320508*0.035
    tab=tab*(1+gr*m)[:,None]
    ds=rrect_(X-8,Y-16,False); sh=np.exp(-((ds+14)/24)**2)*(ds>-60); ca=np.exp(-((ds+36)/9)**2)
    om=1-m; shf=1-0.3025*sh*om; caf=0.06*ca*om
    res=fr*shf[:,None]+caf[:,None]*np.array([1.0,0.93,0.8]); res=res+m[:,None]*(tab-res)
    def tex(dx,dy,want_g=True):
        u=X-384+dx; v=Y-216+dy; dd=bil(T[...,0].astype(np.float64)/255.0,u/1152,v/648)[:,0]*255/8-16
        if not want_g: return dd,None
        xi=np.clip(np.rint(u-0.5).astype(int),0,1151); yi=np.clip(np.rint(v-0.5).astype(int),0,647); return dd,np.rint(T[yi,xi,1].astype(np.float64))
    dt,_=tex(0,0); shade=np.zeros(n); caus=np.zeros(n)
    for g,th in ((1,1.0),(2,0.55),(3,0.33)):
        off=7.0*th; dd,gg=tex(-l2[0]*off,-l2[1]*off); sg=1.5+3*th
        shade=np.maximum(shade,np.where(gg==g,np.clip(0.5-dd/(2.5*sg),0,1),0))
        dd,gg=tex(-l2[0]*off*1.3,-l2[1]*off*1.3); wr=3*th+1+2.5*th
        caus+=np.where((gg==g)&(dd<0),np.exp(-((dd+3*th)/wr)**2),0)
    mi=np.clip(0.5-dt,0,1); omi=1-mi
    res=res*(1-0.40*shade*omi)[:,None]+(strength*0.30*caus*omi)[:,None]*col
    cn=np.maximum(0,1-np.abs(dt-1.2)/1.2); res=res*(1-0.40*cn*omi)[:,None]
    dxp,_=tex(1,0,False); dyp,_=tex(0,1,False); lgx=dxp-dt; lgy=dyp-dt; ll=np.sqrt(lgx*lgx+lgy*lgy)+1e-6; lgx/=ll; lgy/=ll
    lnx,lny,lnz,lh=bev(np.maximum(-dt,0),9.0,lgx,lgy); let=np.zeros((n,3))
    for c in range(3):
        rx,ry,lp,tir=refr(lnx,lny,lnz,lh,1/IORS[c]); let[:,c]=np.where(tir,0.35*ng[:,c],ng[:,c])*np.exp(-ABSC[c]*lp)
    LF=0.04+0.96*(1-lnz)**5; lkr=np.maximum(0,0.45*lnx+0.89*lny); let=let*(1-LF)[:,None]+(LF*(0.10+1.6*lkr**6))[:,None]
    ie=strength*0.55*np.clip(I,0.35,1.6)
    lb=np.maximum(0,1-np.abs(dt+1.5)/1.5); lge=lgx*l2[0]+lgy*l2[1]; lgl=np.maximum(0,-lge); lge=np.maximum(0,lge)
    rims=strength*((lb*lgl**6)*1.1+lge*lb*0.55+0.5*np.exp(-((dt+1.5)/3)**2)*lgl**6)
    let+= (ie+rims)[:,None]*col
    res=res+(mi*m)[:,None]*(let-res)
    lin=np.clip(res,0,1); srgb=np.where(lin<=0.0031308,12.92*lin,1.055*np.power(lin,1/2.4)-0.055)
    dz=np.modf(np.modf(0.0671*X+0.00584*Y)[0]*52.98)[0]; dz=np.where(dz<0,dz+1,dz)
    return srgb+((dz-0.5)/255.0)[:,None]
