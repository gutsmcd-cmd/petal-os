# Build-time generator: font table + built-in files (run by make)
import math, struct, re, glob
from PIL import Image, ImageDraw, ImageFont
# --- built-in images and sounds ---
W,H=160,120
im=Image.new('RGB',(W,H),(226,232,222))
d=ImageDraw.Draw(im)
d.rectangle([0,80,W,H],fill=(157,174,152))
for i in range(5):
    a=i*2*math.pi/5-math.pi/2
    cx,cy=80+22*math.cos(a),55+22*math.sin(a)
    d.ellipse([cx-18,cy-18,cx+18,cy+18],fill=(201,163,168))
d.ellipse([68,43,92,67],fill=(222,206,170))
d.line([80,77,80,118],fill=(110,130,105),width=4)
im.save('files/flower.bmp')
pg=Image.new('RGB',(200,140),(240,236,230));pd=ImageDraw.Draw(pg)
for i in range(7):
    pd.ellipse([10+i*26,30+(i%2)*30,50+i*26,70+(i%2)*30],fill=[(201,163,168),(157,174,152)][i%2])
pd.rectangle([0,118,200,140],fill=(157,174,152))
pg.save('files/petals.png',optimize=True)
def wav(name,notes,rate=11025):
    data=bytearray()
    for f,dur in notes:
        n=int(rate*dur)
        for i in range(n):
            env=math.exp(-3*i/n)
            v=0 if f==0 else math.sin(2*math.pi*f*i/rate)*env
            data.append(int(128+90*v))
    hdr=b'RIFF'+struct.pack('<I',36+len(data))+b'WAVEfmt '+struct.pack('<IHHIIHH',16,1,1,rate,rate,1,8)+b'data'+struct.pack('<I',len(data))
    open('files/'+name,'wb').write(hdr+data)
wav('chime.wav',[(784,.35),(659,.35),(523,.6)])
wav('sakura.wav',[(440,.4),(440,.4),(494,.7),(440,.4),(440,.4),(494,.7),(440,.4),(494,.4),(523,.4),(494,.4),(440,.4),(494,.25),(440,.25),(349,.8)])
# --- files.h ---
out=open('src/files.h','w')
names=sorted(glob.glob('files/*'))
for i,p in enumerate(names):
    b=open(p,'rb').read()
    out.write('static const unsigned char file%d[%d]={%s};\n'%(i,len(b),','.join(str(x) for x in b)))
out.write('static const struct {const char*n;const unsigned char*d;unsigned s;} builtin_files[]={\n')
for i,p in enumerate(names):
    out.write('{"%s",file%d,%d},\n'%(p.split('/')[-1],i,len(open(p,'rb').read())))
out.write('};\n#define NBUILTIN %d\n'%len(names))
# --- font.h ---
text=open('src/kernel.c',encoding='utf-8').read()
for p in names:
    if p.endswith(('.html','.txt')): text+=open(p,encoding='utf-8').read()
cps=sorted(set(range(32,127))|{ord(c) for c in text if ord(c)>126}|{0x2022,0x25B6,0x25A0,0xD7})
f=ImageFont.truetype('/usr/share/fonts/opentype/unifont/unifont_jp.otf',16)
o=open('src/font.h','w')
o.write('#define NGLYPH %d\nstatic const unsigned int g_cp[]={%s};\n'%(len(cps),','.join(map(str,cps))))
ws=[];bm=[]
for c in cps:
    w=int(f.getlength(chr(c))) or 8; w=16 if w>8 else 8
    img=Image.new('L',(16,16),0); ImageDraw.Draw(img).text((0,0),chr(c),font=f,fill=255)
    for y in range(16):
        v=0
        for x in range(16):
            if img.getpixel((x,y))>127: v|=1<<(15-x)
        bm.append(v)
    ws.append(w)
o.write('static const unsigned char g_w[]={%s};\nstatic const unsigned short g_bm[]={%s};\n'%(','.join(map(str,ws)),','.join(map(str,bm))))
