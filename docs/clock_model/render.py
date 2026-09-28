import numpy as np, math
from PIL import Image, ImageDraw, ImageFont
SRC='/tmp/sun1642/20260928_154026_00002526.jpg'
DEMI='/usr/share/fonts/opentype/urw-base35/URWGothic-Demi.otf'; BOOK='/usr/share/fonts/opentype/urw-base35/URWGothic-Book.otf'
W,H=1920,1080
def s2l(c): return np.where(c<=0.04045,c/12.92,((c+0.055)/1.055)**2.4)
def l2s(c): c=np.clip(c,0,1); return np.where(c<=0.0031308,12.92*c,1.055*c**(1/2.4)-0.055)
base=s2l(np.asarray(Image.open(SRC).convert('RGB'),np.float64)/255)
def remove_options_panel(img):
    """clock mode hides the options panel: the sky under it continued by a smooth fit (cubic in x
    and y, least squares) to the star-free sky around it"""
    x1,y1=380,172; o=img.copy()
    ys,xs=np.mgrid[0:320,0:700]; band=~((xs<x1)&(ys<y1))
    reg=img[:320,:700]
    from numpy.lib.stride_tricks import sliding_window_view as swv
    med=np.median(swv(np.pad(reg,((3,3),(3,3),(0,0)),mode='edge'),(7,7),axis=(0,1)),axis=(-2,-1))
    X=xs[band]/700.0; Y=ys[band]/320.0
    A=np.stack([X**i*Y**j for i in range(4) for j in range(4-i)],1)
    yi,xi=np.mgrid[0:y1,0:x1]; Xi=xi/700.0; Yi=yi/320.0
    Ai=np.stack([Xi.ravel()**i*Yi.ravel()**j for i in range(4) for j in range(4-i)],1)
    for c in range(3):
        coef,*_=np.linalg.lstsq(A,med[band][:,c],rcond=None)
        o[:y1,:x1,c]=np.clip((Ai@coef).reshape(y1,x1),0,1)
    return o
def blurred(img):
    """ps_ui's glass source: the frame at 240 x 135, binomial 1-4-6-4-1 H V H V, bilinear back up"""
    s=img.reshape(135,8,240,8,3).mean((1,3)); k=np.array([1,4,6,4,1],np.float64)/16
    for _ in range(2):
        for ax in (1,0):
            p=np.pad(s,[(2,2) if a==ax else (0,0) for a in range(2)]+[(0,0)],mode='edge')
            s=sum(k[i]*np.take(p,range(i,i+s.shape[ax]),axis=ax) for i in range(5))
    up=np.stack([np.asarray(Image.fromarray(np.float32(s[...,c]),'F').resize((W,H),Image.BILINEAR),np.float64) for c in range(3)],2)
    return up
YY,XX=np.mgrid[0:H,0:W]+0.5
def rrect_sdf(cx,cy,hw,hh,r):
    qx=np.maximum(np.abs(XX-cx)-hw+r,0); qy=np.maximum(np.abs(YY-cy)-hh+r,0); return np.sqrt(qx*qx+qy*qy)-r
def glass(img,bl,shapes):
    m=np.zeros((H,W)); e=np.zeros((H,W))
    for (cx,cy,hw,hh,r) in shapes:
        d=rrect_sdf(cx,cy,hw,hh,r); m=np.maximum(m,np.clip(0.5-d,0,1)); e=np.maximum(e,np.maximum(0,1-np.abs(d+0.75)))
    g=bl*0.62+np.array([0.012,0.012,0.016])
    return img+m[...,None]*(g-img)+0.08*e[...,None]
class Layer:
    """UI content drawn 4x supersampled, straight alpha, composited in sRGB space after the encode"""
    def __init__(s,ss=4): s.ss=ss; s.im=Image.new('RGBA',(W*ss,H*ss),(0,0,0,0)); s.d=ImageDraw.Draw(s.im)
    def font(s,path,px): return ImageFont.truetype(path,int(round(px*s.ss)))
    def text(s,x,y,t,f,col=(255,255,255),anchor='ls',shadow=2,sh_a=115/256):
        S=s.ss
        if shadow: s.d.text(((x+shadow)*S,(y+shadow)*S),t,font=f,fill=(0,0,0,int(255*sh_a)),anchor=anchor)
        s.d.text((x*S,y*S),t,font=f,fill=col+(255,),anchor=anchor)
    def tw(s,t,f): b=s.d.textbbox((0,0),t,font=f); return (b[2]-b[0])/s.ss
    def line(s,p0,p1,w,col,a=255):
        S=s.ss; s.d.line([(p0[0]*S,p0[1]*S),(p1[0]*S,p1[1]*S)],fill=col+(a,),width=max(1,int(round(w*S))))
        for p in (p0,p1): s.d.ellipse([(p[0]-w/2)*S,(p[1]-w/2)*S,(p[0]+w/2)*S,(p[1]+w/2)*S],fill=col+(a,))
    def disc(s,cx,cy,r,col,a=255): S=s.ss; s.d.ellipse([(cx-r)*S,(cy-r)*S,(cx+r)*S,(cy+r)*S],fill=col+(a,))
    def arc(s,cx,cy,r,a0,a1,w,col,a=255):
        S=s.ss; s.d.arc([(cx-r)*S,(cy-r)*S,(cx+r)*S,(cy+r)*S],a0,a1,fill=col+(a,),width=max(1,int(round(w*S))))
    def out(s): return np.asarray(s.im.resize((W,H),Image.LANCZOS),np.float64)/255
def compose(lin,layer):
    s=l2s(lin); L=layer.out(); a=L[...,3:4]; return np.clip(L[...,:3]*a+s*(1-a),0,1)
def save(srgb,path): Image.fromarray(np.uint8(np.round(srgb*255))).save(path)
# ---- the always-visible time pill, top centre: the options panel's gap (UI_Y0 10), padding (UI_PAD 22), font
def pill(L,shapes,status='internet'):
    f=L.font(DEMI,19); fs=L.font(DEMI,19)
    t='15:40:26'; st=status; wt=L.tw(t,f); ws=L.tw(st,fs); gap=14; dot=10
    w=22+dot+10+wt+gap+ws+22; h=22+30+22; cx=W/2; top=10
    shapes.append((cx,top+h/2,w/2,h/2,16))
    x=cx-w/2+22; base=top+22+22
    col={'internet':(120,210,140),'console':(235,190,110),'off':(150,150,158)}[status]
    L.disc(x+dot/2+1,base-7+1,dot/2,(0,0,0),115); L.disc(x+dot/2,base-7,dot/2,col)
    x+=dot+10; L.text(x,base,t,f); x+=wt+gap; L.text(x,base,st,fs,col=(205,205,210))
HH,MM,SS=15,40,26
