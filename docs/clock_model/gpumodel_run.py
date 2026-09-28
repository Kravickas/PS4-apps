exec(open('/tmp/clk/gpumodel.py').read())
T=np.frombuffer(open('/tmp/tst/clock_tex.bin','rb').read(),np.uint8).reshape(648,1152,2)
text_d=(T[...,0].astype(float)-128)/8; text_g=T[...,1]
def heavy_frost(scene):   # the extended frost chain: 60 x 34, sigma ~45 px, bilinear back up
    s=gblur(scene,45.0); small=np.stack([np.asarray(Image.fromarray(np.float32(s[...,c]),'F').resize((60,34),Image.BILINEAR),np.float64) for c in range(3)],2)
    return np.stack([np.asarray(Image.fromarray(np.float32(small[...,c]),'F').resize((W,H),Image.BILINEAR),np.float64) for c in range(3)],2)
for name,scene,lxy,colr,stren,fill in (('day',clean,None,Tr(math.sin(math.radians(8.0))),1.35,0.18),('night',np.load('/tmp/clk/night_lin.npy'),(1635,294),np.array([s2l(np.array(0.52)),s2l(np.array(0.64)),s2l(np.array(0.84))])*Tr(0.2539),0.85,0.14)):
    if lxy is None:
        sy_,sx_=np.unravel_index(np.argmax(nd.gaussian_filter(scene.sum(2),25)),scene.shape[:2]); lxy=(sx_,sy_)
    l2=np.array([CX-lxy[0],CY-lxy[1]]); l2/=np.linalg.norm(l2)
    I,tx,ty=internal_light(l2); m=np.clip(0.5-d,0,1); I=I/np.percentile(I[(m>0.5)&(d>-60)&(d<-30)],90)
    img=clock_gpu(scene,heavy_frost(scene),I,tx,ty,lxy,colr,stren,fill,text_d,text_g)
    save(l2s(img),'/tmp/clk/gpumodel_%s.png'%name); print(name)
