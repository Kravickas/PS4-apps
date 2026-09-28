exec(open('/tmp/clk/glass2.py').read())
IOR=(1.514,1.517,1.522)   # BK7-like crown glass at R, G, B (dispersion)
ABS=np.array([0.0010,0.0005,0.0008])   # absorption per px of path (a faint green-cyan edge tint)
def refract(dx,dy,dz,nx,ny,nz,eta):
    c=-(dx*nx+dy*ny+dz*nz); k=1-eta*eta*(1-c*c); tir=k<0; k=np.maximum(k,0)
    f=eta*c-np.sqrt(k); return eta*dx+f*nx,eta*dy+f*ny,eta*dz+f*nz,tir
def slab_faceon(img,d,bevel=38.0,dome=0.0,frost=0.0,dbg=260.0,bbox=None,shadow=True,light=(-0.45,-0.89)):
    """a glass slab seen face-on: height h(s) = quarter circle over the bevel width, flat (plus an
    optional dome) inside; the view ray (0, 0, -1) refracts into the top face, crosses the slab to the
    flat back face at depth h, refracts out and meets the scene dbg px behind: per channel (IOR)."""
    s=np.maximum(-d,0); m=np.clip(0.5-d,0,1)
    u=np.clip(s/bevel,0,1); h=bevel*np.sqrt(np.clip(1-(1-u)**2,0,1))
    if dome>0 and bbox is not None:
        (x0,y0,x1,y1)=bbox; rx=(XX-(x0+x1)/2)/((x1-x0)/2); ry=(YY-(y0+y1)/2)/((y1-y0)/2)
        h=h+dome*np.clip(1-rx*rx,0,1)*np.clip(1-ry*ry,0,1)*u
    gy,gx=np.gradient(nd.gaussian_filter(h,1.0)); nx,ny,nz=-gx,-gy,np.ones_like(h); l=np.sqrt(nx*nx+ny*ny+1); nx,ny,nz=nx/l,ny/l,nz/l
    S=gblur(img,frost) if frost>0 else img
    out=np.zeros_like(img)
    for c,eta in enumerate(IOR):
        rx_,ry_,rz_,tir=refract(0,0,-1,nx,ny,nz,1/eta)
        L=h/np.maximum(np.abs(rz_),1e-3)                   # path inside the glass
        ox=rx_*L; oy=ry_*L
        ex,ey=rx_*eta,ry_*eta; e2=ex*ex+ey*ey; tir2=e2>=1; ez=np.sqrt(np.maximum(1-e2,1e-4))
        ox=ox+ex*dbg/ez; oy=oy+ey*dbg/ez
        smp=nd.map_coordinates(S[...,c],[YY-0.5+oy,XX-0.5+ox],order=1,mode='nearest')
        smp=np.where(tir|tir2,S[...,c]*0.35,smp)           # total internal reflection: the dark glass edge
        out[...,c]=smp*np.exp(-ABS[c]*L)
    # Fresnel (Schlick, F0 0.04) of a soft studio sky: the key light from the top left + the sky's own blur
    F=0.04+0.96*(1-nz)**5; kr=np.maximum(0,-(nx*light[0]+ny*light[1]))
    env=0.10+1.6*kr**6
    out=out*(1-F[...,None])+F[...,None]*env[...,None]
    res=img.copy()
    if shadow:   # a glass shadow: dark where the bevel bends the light away, a caustic just inside it
        rim=np.clip(1-u,0,1)*m; core=np.clip(u*4-3,0,1)*m
        sh=nd.gaussian_filter(np.roll(np.roll(rim,16,0),8,1),16); ca=nd.gaussian_filter(np.roll(np.roll(np.maximum(0,1-np.abs(u-0.82)/0.12)*m,22,0),10,1),6)
        res=res*(1-0.55*sh*(1-m))[...,None]+(0.10*ca*(1-m))[...,None]*np.array([1.0,0.93,0.8])
    return res+m[...,None]*(out-res)
