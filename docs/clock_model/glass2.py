exec(open('/tmp/clk/render.py').read())
import scipy.ndimage as nd
clean=remove_options_panel(base); CX,CY=W/2,H/2
DOT={'ingame':(150,150,158),'console':(240,205,70),'net':(110,215,130)}
def gblur(img,s): return np.stack([nd.gaussian_filter(img[...,c],s,mode='nearest') for c in range(3)],2) if s>0 else img.copy()
def rrect(cx,cy,hw,hh,r): return rrect_sdf(cx,cy,hw,hh,r)
def text_sdf(L_,x,y,t,f,anchor='ms'):
    """signed distance (px, negative inside) of text drawn with the layer's font at 1x"""
    im=Image.new('L',(W*2,H*2),0); d=ImageDraw.Draw(im)
    f2=ImageFont.truetype(f.path,int(round(f.size/L_.ss*2))); d.text((x*2,y*2),t,font=f2,fill=255,anchor=anchor)
    m=np.asarray(im)>127; di=nd.distance_transform_edt(m); do=nd.distance_transform_edt(~m)
    s=(do-di)/2.0; s=s.reshape(H,2,W,2).mean((1,3)); return s
def union(*ds): return np.minimum.reduce(ds)
def glassify(img,d,src_sigma=20,mul=0.7,add=(0.03,0.031,0.036),refr=0.0,refr_w=40,chroma=0.0,
             sheen=0.04,rim=0.06,spec=0.22,rim_w=1.5,shadow=0.3,sh_sig=20,sh_dy=14,grain=0.004,light=(-0.55,-0.83),bbox=None,src=None):
    m=np.clip(0.5-d,0,1)
    gy,gx=np.gradient(nd.gaussian_filter(d,1.2)); gl=np.sqrt(gx*gx+gy*gy)+1e-6; nx,ny=gx/gl,gy/gl
    inside=np.maximum(-d,0); k=np.clip(1-inside/refr_w,0,1)**2
    S=gblur(img if src is None else src,src_sigma)
    out=np.empty_like(img)
    for c,ch in enumerate((-chroma,0,chroma)):
        off=refr+ch
        yy=YY-0.5+ny*off*k; xx=XX-0.5+nx*off*k
        out[...,c]=nd.map_coordinates(S[...,c],[yy,xx],order=1,mode='nearest')
    g=out*mul+np.array(add)
    if bbox is not None:
        y0,y1=bbox; t=np.clip((YY-y0)/(y1-y0),0,1); g=g+sheen*(1-t)[...,None]**2
    band=np.maximum(0,1-np.abs(d+rim_w)/rim_w)
    sp=np.maximum(0,nx*light[0]+ny*light[1]); g=g+(band*(rim+spec*sp))[...,None]
    g=g+(np.random.default_rng(1).normal(0,grain,(H,W)))[...,None]
    res=img.copy()
    if shadow>0:
        sm=nd.gaussian_filter(np.roll(m,int(sh_dy),axis=0),sh_sig); res=res*(1-shadow*sm*(1-m))[...,None]
    return res+m[...,None]*(g-res)
def pill2(L,img,status='net'):
    """the slim top panel: 10 px from the top like the options panel, 44 px tall, the dot + the time"""
    f=L.font(DEMI,19); t='15:40:26'; wt=L.tw(t,f); dot=10; h=44; w=16+dot+9+wt+18; top=10
    img=glass(img,blurred(img),[(CX,top+h/2,w/2,h/2,16)])
    x=CX-w/2+16; base_=top+h/2+7; col=DOT[status]
    L.disc(x+dot/2+1,base_-7+1,dot/2,(0,0,0),115); L.disc(x+dot/2,base_-7,dot/2,col); L.text(x+dot+9,base_,t,f)
    return img
def out(img,L,name):
    save(compose(img,L),'/tmp/clk/%s.png'%name); print(name)
