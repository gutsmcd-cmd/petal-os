/* Petal OS (はなびらOS) - a tiny operating system written from scratch.
   Single kernel: framebuffer GUI, PS/2 input, RAM files, built-in apps. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
typedef unsigned long long u64; typedef int i32;
#include "font.h"
#include "files.h"

/* ---------- low level ---------- */
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8 inb(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
void *memcpy(void*d,const void*s,u32 n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}
void *memset(void*d,int c,u32 n){u8*a=d;while(n--)*a++=(u8)c;return d;}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void scpy(char*d,const char*s,int max){int i=0;while(s[i]&&i<max-1){d[i]=s[i];i++;}d[i]=0;}
static int ends(const char*s,const char*e){int a=slen(s),b=slen(e);return a>=b&&seq(s+a-b,e);}
static void itoa(int v,char*o){char t[12];int n=0,neg=v<0;u32 u=neg?-v:v;do{t[n++]='0'+u%10;u/=10;}while(u);int i=0;if(neg)o[i++]='-';while(n)o[i++]=t[--n];o[i]=0;}
static void itoa2(int v,char*o){o[0]='0'+v/10%10;o[1]='0'+v%10;o[2]=0;}
static u32 rng=12345; static u32 rnd(void){rng=rng*1103515245+12345;return rng>>16;}

/* ---------- timer (PIT ch0 polled) ---------- */
static u32 ms_now; static u16 pit_last; static u32 pit_frac;
static void pit_init(void){outb(0x43,0x34);outb(0x40,0);outb(0x40,0);pit_last=0;}
static void pit_poll(void){outb(0x43,0);u16 c=inb(0x40);c|=inb(0x40)<<8;u16 d=pit_last-c;pit_last=c;pit_frac+=d;ms_now+=pit_frac/1193;pit_frac%=1193;}
static void sleep_ms(u32 m){pit_poll();u32 t=ms_now;while(ms_now-t<m)pit_poll();}

/* ---------- RTC ---------- */
static u8 cmos(u8 r){outb(0x70,r);return inb(0x71);}
static int rtc_s,rtc_m,rtc_h,rtc_d,rtc_mo,rtc_y;
static int clock_jst;
static void rtc_read(void){while(cmos(0x0A)&0x80);u8 b=cmos(0x0B);int v[6]={cmos(0),cmos(2),cmos(4),cmos(7),cmos(8),cmos(9)};
 for(int i=0;i<6;i++){int x=v[i];if(i==2)x&=0x7F;if(!(b&4))x=(x&15)+(x>>4)*10;v[i]=x;}
 if(!(b&2)&&(cmos(4)&0x80))v[2]=(v[2]+12)%24;
 rtc_s=v[0];rtc_m=v[1];rtc_h=v[2];rtc_d=v[3];rtc_mo=v[4];rtc_y=2000+v[5];
 if(clock_jst){rtc_h+=9;if(rtc_h>=24){rtc_h-=24;static const int ml[12]={31,28,31,30,31,30,31,31,30,31,30,31};int L=ml[rtc_mo-1]+(rtc_mo==2&&rtc_y%4==0);if(++rtc_d>L){rtc_d=1;if(++rtc_mo>12){rtc_mo=1;rtc_y++;}}}}}

/* ---------- graphics ---------- */
static u32 *fb; static u32 pitch,SW,SH;
static u32 back[1280*1024];
static int cx0,cy0,cx1,cy1; /* clip */
static void clip_all(void){cx0=0;cy0=0;cx1=SW;cy1=SH;}
static void clip(int x,int y,int w,int h){cx0=x<0?0:x;cy0=y<0?0:y;cx1=x+w>(int)SW?(int)SW:x+w;cy1=y+h>(int)SH?(int)SH:y+h;}
static inline void px(int x,int y,u32 c){if(x>=cx0&&y>=cy0&&x<cx1&&y<cy1)back[y*SW+x]=c;}
static void rect(int x,int y,int w,int h,u32 c){int xa=x<cx0?cx0:x,ya=y<cy0?cy0:y,xb=x+w>cx1?cx1:x+w,yb=y+h>cy1?cy1:y+h;for(int j=ya;j<yb;j++){u32*r=back+j*SW;for(int i=xa;i<xb;i++)r[i]=c;}}
static void frame(int x,int y,int w,int h,u32 c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static void rrect(int x,int y,int w,int h,int r,u32 c){for(int j=0;j<h;j++){int in=0;int dy=j<r?r-j:(j>=h-r?j-(h-r-1):0);if(dy){int dx=0;while(dx<r&&(r-dx)*(r-dx)+dy*dy>r*r)dx++;in=dx;}rect(x+in,y+j,w-2*in,1,c);}}
static u32 mix(u32 a,u32 b,int t){int r=((a>>16&255)*(255-t)+(b>>16&255)*t)/255,g=((a>>8&255)*(255-t)+(b>>8&255)*t)/255,bl=((a&255)*(255-t)+(b&255)*t)/255;return r<<16|g<<8|bl;}
static void line(int x0,int y0,int x1,int y1,int t,u32 c){int dx=x1>x0?x1-x0:x0-x1,dy=y1>y0?y1-y0:y0-y1,sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx-dy;for(;;){rect(x0-t/2,y0-t/2,t,t,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>-dy){e-=dy;x0+=sx;}if(e2<dx){e+=dx;y0+=sy;}}}

/* ---------- muted theme ---------- */
#define C_BG    0xE4E8E1  /* soft grey-green */
#define C_ROSE  0xC9A3A8  /* dusty rose */
#define C_ROSE_D 0xAE8A8F
#define C_ROSE_L 0xEADBDC
#define C_SAGE  0x9DAE98  /* sage */
#define C_SAGE_D 0x7F9079
#define C_SAGE_L 0xD5DDD1
#define C_WIN   0xF5F3EF  /* off-white */
#define C_TEXT  0x4A4C48
#define C_SOFT  0x8A8C86
#define C_LINE  0xCFCFC8
static u32 wall_cols[3]={0xE4E8E1,0xEDE4E3,0xE8E6E0}; static int wall=0;

/* ---------- text ---------- */
static int lang=0; /* 0 en, 1 ja */
#define T(en,ja) (lang?(ja):(en))
static int glyph(u32 cp){int lo=0,hi=NGLYPH-1;while(lo<=hi){int m=(lo+hi)/2;if(g_cp[m]==cp)return m;if(g_cp[m]<cp)lo=m+1;else hi=m-1;}return -1;}
static u32 utf8(const char**s){const u8*p=(const u8*)*s;u32 c=*p++;if(c>=0xF0){c=(c&7)<<18|(p[0]&63)<<12|(p[1]&63)<<6|(p[2]&63);p+=3;}else if(c>=0xE0){c=(c&15)<<12|(p[0]&63)<<6|(p[1]&63);p+=2;}else if(c>=0xC0){c=(c&31)<<6|(p[0]&63);p++;}*s=(const char*)p;return c;}
static int cpw(u32 cp){int g=glyph(cp);return g<0?(cp>127?16:8):g_w[g];}
static int textwn(const char*s,int n,int sc){int w=0;const char*e=s+n;while(*s&&s<e)w+=cpw(utf8(&s))*sc;return w;}
static int textw(const char*s){return textwn(s,9999,1);}
static int drawch(int x,int y,u32 cp,u32 col,int sc){int g=glyph(cp);int w=g<0?8:g_w[g];if(g<0){frame(x+1,y+3,6*sc,10*sc,col);return w*sc;}
 for(int j=0;j<16;j++){u16 r=g_bm[g*16+j];if(!r)continue;for(int i=0;i<w;i++)if(r&(1<<(15-i))){if(sc==1)px(x+i,y+j,col);else rect(x+i*sc,y+j*sc,sc,sc,col);}}return w*sc;}
static int textn(int x,int y,const char*s,int n,u32 c,int sc){const char*e=s+n;while(*s&&s<e)x+=drawch(x,y,utf8(&s),c,sc);return x;}
static int text(int x,int y,const char*s,u32 c){return textn(x,y,s,9999,c,1);}
static void textc(int x,int y,int w,const char*s,u32 c){text(x+(w-textw(s))/2,y,s,c);}

/* ---------- RAM filesystem ---------- */
#define MAXF 48
typedef struct{char name[32];const u8*data;u32 size;}File;
static File files[MAXF];static int nfiles;
static u8 pool[1<<20];static u32 pool_used;
static void fs_init(void){for(int i=0;i<NBUILTIN;i++){scpy(files[i].name,builtin_files[i].n,32);files[i].data=builtin_files[i].d;files[i].size=builtin_files[i].s;}nfiles=NBUILTIN;}
static File*fs_find(const char*n){for(int i=0;i<nfiles;i++)if(seq(files[i].name,n))return &files[i];return 0;}
static File*fs_write(const char*n,const u8*d,u32 s){File*f=fs_find(n);if(!f){if(nfiles>=MAXF)return 0;f=&files[nfiles++];scpy(f->name,n,32);}
 if(pool_used+s>sizeof(pool))pool_used=0; /* tiny pool: wrap (demo) */
 u8*p=pool+pool_used;memcpy(p,d,s);pool_used+=(s+3)&~3;f->data=p;f->size=s;return f;}

/* ---------- BMP ---------- */
static u32 rd32(const u8*p){return p[0]|p[1]<<8|p[2]<<16|(u32)p[3]<<24;}
static int bmp_size0(const u8*b,int*w,int*h){if(b[0]!='B'||b[1]!='M')return 0;*w=rd32(b+18);*h=(i32)rd32(b+22);if(*h<0)*h=-*h;return b[28]==24||b[28]==32;}
static void bmp_draw0(const u8*b,int x,int y,int sc){int w,h;if(!bmp_size0(b,&w,&h))return;int bpp=b[28]/8;int top=(i32)rd32(b+22)<0;const u8*d=b+rd32(b+10);int st=(w*bpp+3)&~3;
 for(int j=0;j<h;j++){const u8*r=d+(top?j:h-1-j)*st;for(int i=0;i<w;i++){u32 c=r[i*bpp+2]<<16|r[i*bpp+1]<<8|r[i*bpp];if(sc==1)px(x+i,y+j,c);else rect(x+i*sc,y+j*sc,sc,sc,c);}}}
/* ---------- PNG (tiny inflate) ---------- */
static const u8*zin;static u32 zpos,zbit;
static int zgetbit(void){int b=(zin[zpos]>>zbit)&1;if(++zbit==8){zbit=0;zpos++;}return b;}
static u32 zbits(int n){u32 v=0;for(int i=0;i<n;i++)v|=(u32)zgetbit()<<i;return v;}
typedef struct{u16 cnt[16],sym[320];}Huff;
static void hbuild(Huff*h,const u8*len,int n){int offs[16];for(int i=0;i<16;i++)h->cnt[i]=0;for(int i=0;i<n;i++)h->cnt[len[i]]++;h->cnt[0]=0;offs[1]=0;for(int i=1;i<15;i++)offs[i+1]=offs[i]+h->cnt[i];for(int i=0;i<n;i++)if(len[i])h->sym[offs[len[i]]++]=i;}
static int hdecode(Huff*h){int code=0,first=0,idx=0;for(int l=1;l<16;l++){code|=zgetbit();int c=h->cnt[l];if(code-c<first)return h->sym[idx+(code-first)];idx+=c;first+=c;first<<=1;code<<=1;}return -1;}
static const u16 lbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const u8 lext[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const u16 dbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const u8 dext[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static Huff hl,hd,hc;
static u32 zinflate(const u8*in,u8*out,u32 omax){zin=in;zpos=zbit=0;u32 o=0;int fin;u8 len[320];
 do{fin=zbits(1);int t=zbits(2);
  if(t==0){if(zbit){zbit=0;zpos++;}u32 n=zin[zpos]|zin[zpos+1]<<8;zpos+=4;while(n--&&o<omax)out[o++]=zin[zpos++];continue;}
  if(t==1){for(int i=0;i<288;i++)len[i]=i<144?8:i<256?9:i<280?7:8;hbuild(&hl,len,288);for(int i=0;i<30;i++)len[i]=5;hbuild(&hd,len,30);}
  else if(t==2){int nl=zbits(5)+257,nd=zbits(5)+1,nc=zbits(4)+4;static const u8 ord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};u8 cl[19];for(int i=0;i<19;i++)cl[i]=0;for(int i=0;i<nc;i++)cl[ord[i]]=zbits(3);hbuild(&hc,cl,19);
   int i=0;while(i<nl+nd){int sy=hdecode(&hc);if(sy<0)return 0;if(sy<16)len[i++]=sy;else{int r,v=0;if(sy==16){if(!i)return 0;v=len[i-1];r=3+zbits(2);}else if(sy==17)r=3+zbits(3);else r=11+zbits(7);while(r--&&i<320)len[i++]=v;}}
   hbuild(&hl,len,nl);hbuild(&hd,len+nl,nd);}
  else return 0;
  for(;;){int sy=hdecode(&hl);if(sy<0||sy>285)return 0;if(sy<256){if(o<omax)out[o++]=sy;}else if(sy==256)break;else{sy-=257;u32 l=lbase[sy]+zbits(lext[sy]);int ds=hdecode(&hd);if(ds<0||ds>29)return 0;u32 d=dbase[ds]+zbits(dext[ds]);if(d>o)return 0;while(l--&&o<omax){out[o]=out[o-d];o++;}}}
 }while(!fin);return o;}
static u32 be32(const u8*p){return (u32)p[0]<<24|p[1]<<16|p[2]<<8|p[3];}
#define PNGMAXW 640
#define PNGMAXH 480
static u32 png_px[PNGMAXW*PNGMAXH];static u8 png_raw[(PNGMAXW*4+1)*PNGMAXH],png_z[1<<20];static const u8*png_cur;static int png_w,png_h;
static int png_load(const u8*b){if(png_cur==b)return png_w>0;png_cur=b;png_w=0;
 static const u8 sig[8]={137,80,78,71,13,10,26,10};for(int i=0;i<8;i++)if(b[i]!=sig[i])return 0;
 u32 p=8,zl=0;int w=0,h=0,ct=0,bd=0,il=0;static u8 pal[768];
 for(int guard=0;guard<100000;guard++){u32 n=be32(b+p);const u8*t=b+p+4,*d=b+p+8;
  if(t[0]=='I'&&t[1]=='H'){w=be32(d);h=be32(d+4);bd=d[8];ct=d[9];il=d[12];}
  else if(t[0]=='P'&&t[1]=='L'&&t[2]=='T'){memcpy(pal,d,n<768?n:768);}
  else if(t[0]=='I'&&t[1]=='D'&&t[2]=='A'){if(zl+n<=sizeof png_z){memcpy(png_z+zl,d,n);zl+=n;}}
  else if(t[0]=='I'&&t[1]=='E'&&t[2]=='N')break;
  p+=12+n;}
 if(bd!=8||il||w<=0||h<=0||w>PNGMAXW||h>PNGMAXH)return 0;
 int bpp=ct==2?3:ct==6?4:ct==3?1:ct==0?1:ct==4?2:0;if(!bpp)return 0;
 u32 st=w*bpp,need=(st+1)*h;if(zinflate(png_z+2,png_raw,need)<need)return 0;
 u8*prev=0;for(int y=0;y<h;y++){u8*c=png_raw+y*(st+1)+1;int f=c[-1];
  for(u32 i=0;i<st;i++){int a=i>=(u32)bpp?c[i-bpp]:0,up=prev?prev[i]:0,ul=(prev&&i>=(u32)bpp)?prev[i-bpp]:0,v=c[i];
   if(f==1)v+=a;else if(f==2)v+=up;else if(f==3)v+=(a+up)/2;else if(f==4){int pp=a+up-ul,pa=pp>a?pp-a:a-pp,pb=pp>up?pp-up:up-pp,pc=pp>ul?pp-ul:ul-pp;v+=(pa<=pb&&pa<=pc)?a:pb<=pc?up:ul;}
   c[i]=v;}
  for(int x=0;x<w;x++){u8*q=c+x*bpp;u32 R,G,B;if(ct==3){R=pal[q[0]*3];G=pal[q[0]*3+1];B=pal[q[0]*3+2];}else if(ct==0||ct==4){R=G=B=q[0];}else{R=q[0];G=q[1];B=q[2];}
   if(ct==6||ct==4){u32 A=q[bpp-1];R=(R*A+0xFF*(255-A))/255;G=(G*A+0xFF*(255-A))/255;B=(B*A+0xFF*(255-A))/255;}
   png_px[y*w+x]=R<<16|G<<8|B;}
  prev=c;}
 png_w=w;png_h=h;return 1;}
static int bmp_size(const u8*b,int*w,int*h){if(b[0]==137&&b[1]=='P'){if(!png_load(b))return 0;*w=png_w;*h=png_h;return 1;}return bmp_size0(b,w,h);}
static void bmp_draw(const u8*b,int x,int y,int sc){if(b[0]==137&&b[1]=='P'){if(!png_load(b))return;for(int j=0;j<png_h;j++)for(int i=0;i<png_w;i++){u32 c=png_px[j*png_w+i];if(sc==1)px(x+i,y+j,c);else rect(x+i*sc,y+j*sc,sc,sc,c);}return;}bmp_draw0(b,x,y,sc);}

/* ---------- sound: PC speaker + Sound Blaster 16 ---------- */
static void spk(u32 f){if(!f){outb(0x61,inb(0x61)&0xFC);return;}u32 d=1193180/f;outb(0x43,0xB6);outb(0x42,d);outb(0x42,d>>8);outb(0x61,inb(0x61)|3);}
static const u16 tune_sakura[]={440,400,440,400,494,700,0,100,440,400,440,400,494,700,0,100,440,400,494,400,523,400,494,400,440,400,494,250,440,250,349,800,0,0};
static const u16 tune_happy[]={523,250,523,250,587,500,523,500,698,500,659,900,0,150,523,250,523,250,587,500,523,500,784,500,698,900,0,0};
static const u16*tune;static int tune_i;static u32 tune_t;
static void tune_play(const u16*t){tune=t;tune_i=0;tune_t=ms_now;spk(t[0]);}
static void tune_stop(void){tune=0;spk(0);}
static void tune_poll(void){if(!tune)return;if(ms_now-tune_t>=tune[tune_i*2+1]){tune_i++;tune_t=ms_now;if(!tune[tune_i*2+1]){tune_stop();return;}spk(tune[tune_i*2]);}}
static int sb_ok;static u8 dma_buf[65536] __attribute__((aligned(65536)));
static u32 sb_start,sb_len;static int sb_playing;
static void sb_w(u8 v){int t=100000;while((inb(0x22C)&0x80)&&t--);outb(0x22C,v);}
static void sb_init(void){outb(0x226,1);for(volatile int i=0;i<10000;i++);outb(0x226,0);for(int t=0;t<100000;t++)if(inb(0x22E)&0x80){if(inb(0x22A)==0xAA){sb_ok=1;sb_w(0xD1);}return;}}
static int wav_play(const u8*w,u32 size){if(size<44||w[0]!='R')return 0;u32 rate=rd32(w+24);const u8*p=w+12;const u8*end=w+size;while(p+8<end&&!(p[0]=='d'&&p[1]=='a'&&p[2]=='t'&&p[3]=='a'))p+=8+rd32(p+4);if(p+8>=end)return 0;
 u32 len=rd32(p+4);if(len>65535)len=65535;
 if(!sb_ok){return 0;}
 memcpy(dma_buf,p+8,len);u32 a=(u32)dma_buf;
 outb(0x0A,5);outb(0x0C,0);outb(0x0B,0x49);outb(0x02,a);outb(0x02,a>>8);outb(0x83,a>>16);outb(0x03,(len-1));outb(0x03,(len-1)>>8);outb(0x0A,1);
 sb_w(0x40);sb_w(256-1000000/rate);sb_w(0x14);sb_w((len-1)&255);sb_w((len-1)>>8);
 sb_start=ms_now;sb_len=len*1000/rate;sb_playing=1;return 1;}
static void wav_stop(void){if(sb_ok&&sb_playing){sb_w(0xD0);}sb_playing=0;}

/* ---------- input ---------- */
enum{K_UP=0x100,K_DOWN,K_LEFT,K_RIGHT,K_F1=0x110};
static int mx=400,my=300,mbtn,mprev;
static int shift,ext;
static const char*km_lo="\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char*km_hi="\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static void ps2_wait_w(void){for(int t=0;t<100000&&(inb(0x64)&2);t++);}
static void ps2_cmd(u8 c){ps2_wait_w();outb(0x64,c);}
static void ps2_data(u8 d){ps2_wait_w();outb(0x60,d);}
static void ps2_flush(void){for(int t=0;t<1000&&(inb(0x64)&1);t++)inb(0x60);}
static void mouse_init(void){ps2_flush();ps2_cmd(0xA8);ps2_cmd(0x20);for(int t=0;t<100000&&!(inb(0x64)&1);t++);u8 s=inb(0x60);s|=2;s&=~0x20;s&=~3;/* polled: no IRQs */s|=0x40;ps2_cmd(0x60);ps2_data(s);
 ps2_cmd(0xD4);ps2_data(0xF6);sleep_ms(5);ps2_flush();ps2_cmd(0xD4);ps2_data(0xF4);sleep_ms(5);ps2_flush();}
static u8 mpk[3];static int mpi;
static void on_key(int k);static void on_mouse(void);
static void input_poll(void){for(int n=0;n<64;n++){u8 st=inb(0x64);if(!(st&1))break;u8 d=inb(0x60);
 if(st&0x20){if(mpi==0&&!(d&8))continue;mpk[mpi++]=d;if(mpi==3){mpi=0;int dx=mpk[1]-((mpk[0]<<4)&0x100),dy=mpk[2]-((mpk[0]<<3)&0x100);
   mx+=dx;my-=dy;if(mx<0)mx=0;if(my<0)my=0;if(mx>=(int)SW)mx=SW-1;if(my>=(int)SH)my=SH-1;mbtn=mpk[0]&1;on_mouse();}continue;}
 if(d==0xE0){ext=1;continue;}
 if(d==0x2A||d==0x36){shift=1;continue;}if(d==0xAA||d==0xB6){shift=0;continue;}
 if(d&0x80){ext=0;continue;}
 int k=0;if(ext){ext=0;if(d==0x48)k=K_UP;else if(d==0x50)k=K_DOWN;else if(d==0x4B)k=K_LEFT;else if(d==0x4D)k=K_RIGHT;}
 else if(d>=0x3B&&d<=0x44)k=K_F1+d-0x3B;else if(d==0x57)k=K_F1+10;else if(d==0x58)k=K_F1+11;
 else if(d<58)k=(u8)(shift?km_hi:km_lo)[d];
 if(k)on_key(k);}}

/* ---------- widgets ---------- */
static int clickx,clicky,clicked; /* local click for current app frame */
static int button(int x,int y,int w,int h,const char*s,int on){int hov=clicked&&clickx>=x&&clickx<x+w&&clicky>=y&&clicky<y+h;
 rrect(x,y,w,h,5,on?C_ROSE:C_SAGE_L);textc(x,y+(h-16)/2,w,s,on?0xFFFFFF:C_TEXT);if(hov)clicked=0;return hov;}
static int hit(int x,int y,int w,int h){if(clicked&&clickx>=x&&clickx<x+w&&clicky>=y&&clicky<y+h){clicked=0;return 1;}return 0;}

/* ---------- apps & windows ---------- */
typedef struct{const char*en,*ja,*ic;int w,h;u32 col;void(*draw)(int,int,int,int);void(*key)(int);void(*drag)(int,int,int);}App;
enum{A_FILES,A_NOTES,A_CALC,A_CAL,A_MEDIA,A_PAINT,A_WEB,A_SET,A_SNAKE,A_TTT,NAPP};
extern App apps[NAPP];
typedef struct{int open,x,y,w,h;}Win;static Win wins[NAPP];static int zorder[NAPP],nz; /* zorder[nz-1] = front */
static void focus(int a){int j=0;for(int i=0;i<nz;i++)if(zorder[i]!=a)zorder[j++]=zorder[i];zorder[j++]=a;nz=j;}
static void open_app(int a){if(!wins[a].open){wins[a].open=1;if(!wins[a].w){wins[a].w=apps[a].w;wins[a].h=apps[a].h;}static int cascade;wins[a].x=230+(cascade%6)*30+ (a==A_SNAKE?120:0);wins[a].y=40+(cascade%6)*26;cascade++;
  if(wins[a].x+wins[a].w>(int)SW)wins[a].x=SW-wins[a].w-10;if(wins[a].y+wins[a].h+30>(int)SH-40)wins[a].y=10;}focus(a);}
static void close_app(int a){wins[a].open=0;int j=0;for(int i=0;i<nz;i++)if(zorder[i]!=a)zorder[j++]=zorder[i];nz=j;if(a==A_MEDIA){wav_stop();tune_stop();}}
static int front(void){return nz?zorder[nz-1]:-1;}
#define TB 28  /* title bar */

/* ===== Notes ===== */
static char note[8192];static int note_len,note_cur;static char note_file[32]="note.txt";static char note_msg[48];static int note_scroll;
static void notes_load(const char*n){File*f=fs_find(n);if(!f)return;u32 s=f->size<8191?f->size:8191;memcpy(note,f->data,s);note[s]=0;note_len=s;note_cur=s;scpy(note_file,n,32);note_msg[0]=0;note_scroll=0;}
static void notes_draw(int x,int y,int w,int h){
 if(button(x+8,y+6,70,26,T("New","新規"),0)){note_len=note_cur=0;note[0]=0;scpy(note_file,"note.txt",32);note_msg[0]=0;}
 if(button(x+84,y+6,70,26,T("Save","保存"),1)){fs_write(note_file,(u8*)note,note_len);scpy(note_msg,T("Saved!","保存しました"),48);}
 text(x+168,y+11,note_file,C_SOFT);text(x+w-textw(note_msg)-10,y+11,note_msg,C_SAGE_D);
 rect(x+8,y+40,w-16,h-48,0xFFFFFF);frame(x+8,y+40,w-16,h-48,C_LINE);
 clip(x+12,y+44,w-24,h-56);int lx=x+14,ly=y+46-note_scroll*18,maxx=x+w-22;const char*s=note;
 for(int i=0;i<=note_len;i++){if(i==note_cur)rect(lx,ly,2,16,C_ROSE_D);if(i==note_len)break;const char*p=note+i;if(*p=='\n'){lx=x+14;ly+=18;continue;}
  const char*q=p;u32 cp=utf8(&q);int cw=cpw(cp);if(lx+cw>maxx){lx=x+14;ly+=18;}drawch(lx,ly,cp,C_TEXT,1);lx+=cw;i+=(q-p)-1;}
 (void)s;if(ly>y+h-30)note_scroll++;}
static void notes_key(int k){if(k=='\b'){if(note_cur>0){memcpy(note+note_cur-1,note+note_cur,note_len-note_cur+1);note_cur--;note_len--;}}
 else if(k==K_LEFT){if(note_cur)note_cur--;}else if(k==K_RIGHT){if(note_cur<note_len)note_cur++;}
 else if(k==K_UP){if(note_scroll)note_scroll--;}
 else if((k>=32&&k<127)||k=='\n'){if(note_len<8190){for(int i=note_len;i>=note_cur;i--)note[i+1]=note[i];note[note_cur++]=k;note_len++;}}
 note_msg[0]=0;}

/* ===== Browser ===== */
static char web_page[32]="home.html";static char web_hist[8][32];static int web_nh,web_scroll;
typedef struct{int x,y,w,h;char href[32];}Link;static Link links[32];static int nlinks;
static void web_go(const char*p){if(web_nh<8)scpy(web_hist[web_nh++],web_page,32);scpy(web_page,p,32);web_scroll=0;}
static void web_render(int x0,int y0,int w,int maxy){File*f=fs_find(web_page);nlinks=0;int X=x0,Y=y0-web_scroll,lh=18;
 if(!f){text(X,Y,T("Page not found.","ページが見つかりません。"),C_ROSE_D);return;}
 const char*s=(const char*)f->data,*e=s+f->size;int sc=1;u32 col=C_TEXT;int inlink=0;char href[32]="";int bold=0;
 while(s<e){if(*s=='<'){const char*t=s+1;int close=*t=='/';if(close)t++;char tag[8];int n=0;while(t<e&&*t!='>'&&*t!=' '&&n<7)tag[n++]=*t++;tag[n]=0;
   const char*gt=t;while(gt<e&&*gt!='>')gt++;
   char src[32]="";const char*a=t;while(a<gt){if((a[0]=='h'&&a[1]=='r')||(a[0]=='s'&&a[1]=='r')){while(a<gt&&*a!='"')a++;a++;int m=0;while(a<gt&&*a!='"'&&m<31)src[m++]=*a++;src[m]=0;break;}a++;}
   if(seq(tag,"h1")||seq(tag,"h2")||seq(tag,"p")||seq(tag,"li")||seq(tag,"br")){if(X!=x0||close){X=x0;Y+=lh;}if(!close&&!seq(tag,"br"))Y+=6;}
   if(seq(tag,"h1")){sc=close?1:2;col=close?C_TEXT:C_ROSE_D;lh=close?18:34;}
   else if(seq(tag,"h2")){bold=!close;col=close?C_TEXT:C_SAGE_D;}
   else if(seq(tag,"li")&&!close){X=x0+6;drawch(X,Y,0x2022,C_ROSE,1);X+=16;}
   else if(seq(tag,"a")){inlink=!close;if(!close)scpy(href,src,32);}
   else if(seq(tag,"img")){File*im=fs_find(src);int iw,ih;if(im&&bmp_size(im->data,&iw,&ih)){if(X!=x0){X=x0;Y+=lh;}bmp_draw(im->data,X,Y+4,1);Y+=ih+8;}}
   s=gt+1;continue;}
  if(*s=='\n'||*s=='\r'){s++;if(X>x0&&s<e&&*s!='<'){X+=8;}continue;}
  /* one word */
  const char*w0=s;char word[64];int n=0;while(s<e&&*s!='<'&&*s!=' '&&*s!='\n'&&n<60){if(*s=='&'){const char*q=s;while(q<e&&*q!=';'&&q-s<6)q++;char c='&';if(s[1]=='l')c='<';else if(s[1]=='g')c='>';word[n++]=c;s=q+1;continue;}word[n++]=*s++;}word[n]=0;(void)w0;
  int ww=textw(word)*sc;if(X+ww>x0+w&&X>x0){X=x0;Y+=lh;}
  u32 c=inlink?C_ROSE_D:col;if(Y>-40&&Y<maxy){textn(X,Y,word,99,c,sc);if(bold)textn(X+1,Y,word,99,c,sc);if(inlink)rect(X,Y+15,ww,1,c);}
  if(inlink&&nlinks<32){links[nlinks].x=X;links[nlinks].y=Y;links[nlinks].w=ww+8;links[nlinks].h=16;scpy(links[nlinks].href,href,32);nlinks++;}
  X+=ww;if(s<e&&*s==' '){X+=8*sc;s++;}}}
static void web_draw(int x,int y,int w,int h){
 if(button(x+8,y+6,60,26,T("Back","戻る"),0)&&web_nh){scpy(web_page,web_hist[--web_nh],32);web_scroll=0;}
 if(button(x+74,y+6,60,26,T("Home","ホーム"),0))web_go("home.html");
 rrect(x+142,y+6,w-150,26,5,0xFFFFFF);text(x+152,y+11,"petal://",C_SOFT);text(x+152+64,y+11,web_page,C_TEXT);
 rect(x,y+40,w,h-40,0xFFFFFF);clip(x,y+40,w,h-40);
 int lc=clicked;int lx=clickx,ly=clicky;web_render(x+18,y+52,w-40,y+h);
 if(lc)for(int i=0;i<nlinks;i++)if(lx>=links[i].x&&lx<links[i].x+links[i].w&&ly>=links[i].y&&ly<links[i].y+links[i].h){web_go(links[i].href);clicked=0;break;}}
static void web_key(int k){if(k==K_DOWN)web_scroll+=36;if(k==K_UP&&web_scroll>0)web_scroll-=36;if(k=='\b'&&web_nh){scpy(web_page,web_hist[--web_nh],32);web_scroll=0;}}

/* ===== Calculator ===== */
static char calc_disp[24]="0";static double calc_acc;static int calc_op,calc_new=1;
static double calc_val(void){double v=0,f=0;int neg=0;const char*s=calc_disp;if(*s=='-'){neg=1;s++;}for(;*s;s++){if(*s=='.'){f=1;continue;}if(f){f/=10;v+=(*s-'0')*f;}else v=v*10+(*s-'0');}return neg?-v:v;}
static void calc_show(double v){if(v!=v||v>1e15||v<-1e15){scpy(calc_disp,T("Error","エラー"),24);calc_new=1;return;}char o[24];int n=0;if(v<0){o[n++]='-';v=-v;}
 u64 ip=(u64)v;double fr=v-ip;u64 fp=(u64)(fr*1000000+0.5);if(fp>=1000000){ip++;fp-=1000000;}char t[24];int m=0;do{t[m++]='0'+ip%10;ip/=10;}while(ip);while(m)o[n++]=t[--m];
 if(fp){o[n++]='.';char d[7];for(int i=5;i>=0;i--){d[i]='0'+fp%10;fp/=10;}int L=6;while(L&&d[L-1]=='0')L--;for(int i=0;i<L;i++)o[n++]=d[i];}o[n]=0;scpy(calc_disp,o,24);}
static void calc_press(int c){double v=calc_val();
 if((c>='0'&&c<='9')||c=='.'){if(calc_new){calc_disp[0]=0;calc_new=0;}int n=slen(calc_disp);if(c=='.'){for(int i=0;i<n;i++)if(calc_disp[i]=='.')return;if(!n)calc_disp[n++]='0';}if(n<15){calc_disp[n]=c;calc_disp[n+1]=0;}if(seq(calc_disp,"00"))scpy(calc_disp,"0",24);}
 else if(c=='C'){scpy(calc_disp,"0",24);calc_acc=0;calc_op=0;calc_new=1;}
 else if(c=='\b'){int n=slen(calc_disp);if(n>1)calc_disp[n-1]=0;else scpy(calc_disp,"0",24);}
 else if(c=='%'){calc_show(v/100);calc_new=1;}
 else{ if(calc_op&&!calc_new){double a=calc_acc;if(calc_op=='+')a+=v;if(calc_op=='-')a-=v;if(calc_op=='*')a*=v;if(calc_op=='/'){if(v==0){scpy(calc_disp,T("Error","エラー"),24);calc_op=0;calc_new=1;return;}a/=v;}calc_show(a);v=a;}
  calc_acc=v;calc_op=(c=='='||c=='\n')?0:c;calc_new=1;}}
static void calc_draw(int x,int y,int w,int h){(void)h;rrect(x+12,y+10,w-24,56,8,0xFFFFFF);
 if(calc_op){char o[2]={calc_op=='*'?'x':calc_op,0};text(x+22,y+14,o,C_SOFT);}
 int tw=textw(calc_disp)*2;textn(x+w-24-tw,y+22,calc_disp,99,C_TEXT,2);
 const char*k="C%\b/789*456-123+00.=";const char*lab[]={"C","%","<","÷","7","8","9","×","4","5","6","-","1","2","3","+","0","00",".","="};
 int bw=(w-24-18)/4,bh=46;for(int i=0;i<20;i++){int bx=x+12+(i%4)*(bw+6),by=y+76+(i/4)*(bh+6);int op=(i%4==3)||i<3;
  rrect(bx,by,bw,bh,8,i==19?C_ROSE:(op?C_SAGE_L:0xFFFFFF));const char*l=lab[i];if(i==0)l=T("C","C");textc(bx,by+15,bw,l,i==19?0xFFFFFF:C_TEXT);
  if(hit(bx,by,bw,bh)){if(i==17){calc_press('0');calc_press('0');}else calc_press(k[i]);}}}
static void calc_key(int k){if(k=='x'||k=='X')k='*';if(k==27)return;if((k>='0'&&k<='9')||k=='.'||k=='+'||k=='-'||k=='*'||k=='/'||k=='='||k=='\n'||k=='\b'||k=='%'||k=='c'||k=='C')calc_press(k=='c'?'C':k);}

/* ===== Calendar ===== */
static int cal_y,cal_m;
static int dow(int y,int m,int d){static int t[]={0,3,2,5,0,3,5,1,4,6,2,4};if(m<3)y--;return (y+y/4-y/100+y/400+t[m-1]+d)%7;}
static int mdays(int y,int m){static int d[]={31,28,31,30,31,30,31,31,30,31,30,31};return m==2&&((y%4==0&&y%100)||y%400==0)?29:d[m-1];}
static void cal_draw(int x,int y,int w,int h){(void)h;if(!cal_y){cal_y=rtc_y;cal_m=rtc_mo;}
 char t[16],b[4];itoa2(rtc_h,b);scpy(t,b,16);t[2]=':';itoa2(rtc_m,t+3);t[5]=':';itoa2(rtc_s,t+6);
 textn(x+(w-textw(t)*3)/2,y+10,t,99,C_TEXT,3);
 static const char*mn_en[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
 static const char*wd_en[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};static const char*wd_ja[]={"日","月","火","水","木","金","土"};
 char d[48],n[8];if(lang){itoa(rtc_y,d);scpy(d+slen(d),"年",8);itoa(rtc_mo,n);scpy(d+slen(d),n,8);scpy(d+slen(d),"月",8);itoa(rtc_d,n);scpy(d+slen(d),n,8);scpy(d+slen(d),"日 (",8);scpy(d+slen(d),wd_ja[dow(rtc_y,rtc_mo,rtc_d)],8);scpy(d+slen(d),")",4);}
 else{scpy(d,wd_en[dow(rtc_y,rtc_mo,rtc_d)],48);scpy(d+slen(d)," ",4);itoa(rtc_d,n);scpy(d+slen(d),n,8);scpy(d+slen(d)," ",4);scpy(d+slen(d),mn_en[rtc_mo-1],16);scpy(d+slen(d)," ",4);itoa(rtc_y,n);scpy(d+slen(d),n,8);}
 textc(x,y+64,w,d,C_SOFT);
 int gy=y+96;if(button(x+12,gy,36,26,"<",0)){if(--cal_m<1){cal_m=12;cal_y--;}}if(button(x+w-48,gy,36,26,">",0)){if(++cal_m>12){cal_m=1;cal_y++;}}
 if(lang){itoa(cal_y,d);scpy(d+slen(d),"年 ",8);itoa(cal_m,n);scpy(d+slen(d),n,8);scpy(d+slen(d),"月",8);}else{scpy(d,mn_en[cal_m-1],48);scpy(d+slen(d)," ",4);itoa(cal_y,n);scpy(d+slen(d),n,8);}
 textc(x,gy+5,w,d,C_ROSE_D);
 int cw=(w-24)/7;for(int i=0;i<7;i++)textc(x+12+i*cw,gy+38,cw,T(wd_en[i],wd_ja[i]),i==0?C_ROSE_D:(i==6?C_SAGE_D:C_SOFT));
 int st=dow(cal_y,cal_m,1),nd=mdays(cal_y,cal_m);for(int dd=1;dd<=nd;dd++){int c=(st+dd-1)%7,r=(st+dd-1)/7;int bx=x+12+c*cw,by=gy+62+r*36;
  int today=dd==rtc_d&&cal_m==rtc_mo&&cal_y==rtc_y;if(today)rrect(bx+4,by,cw-8,30,8,C_ROSE);itoa(dd,n);textc(bx,by+7,cw,n,today?0xFFFFFF:C_TEXT);}}
static void cal_key(int k){if(k==K_LEFT){if(--cal_m<1){cal_m=12;cal_y--;}}if(k==K_RIGHT){if(++cal_m>12){cal_m=1;cal_y++;}}}

/* ===== Files ===== */
static int files_sel=-1;
static void open_file(const char*n);
static void files_draw(int x,int y,int w,int h){textc(x,y+8,w,T("Everything saved on this computer","このパソコンに保存されているもの"),C_SOFT);
 rect(x+10,y+32,w-20,1,C_LINE);text(x+50,y+38,T("Name","名前"),C_SOFT);text(x+w-110,y+38,T("Size","サイズ"),C_SOFT);
 for(int i=0;i<nfiles&&y+60+i*28<y+h-10;i++){int ry=y+58+i*28;if(i==files_sel)rrect(x+10,ry-4,w-20,26,6,C_ROSE_L);
  const char*nm=files[i].name;u32 ic=ends(nm,".txt")?C_SAGE:ends(nm,".html")?0xB9B39A:(ends(nm,".bmp")||ends(nm,".png"))?C_ROSE:0xA9A6C0;rrect(x+20,ry,18,18,4,ic);
  text(x+50,ry+1,nm,C_TEXT);char s[16];u32 z=files[i].size;if(z<1024){itoa(z,s);scpy(s+slen(s)," B",4);}else{itoa(z/1024,s);scpy(s+slen(s)," KB",4);}text(x+w-110,ry+1,s,C_SOFT);
  if(hit(x+10,ry-4,w-20,26)){files_sel=i;open_file(nm);}}}
static void files_key(int k){if(k==K_DOWN&&files_sel<nfiles-1)files_sel++;if(k==K_UP&&files_sel>0)files_sel--;if(k=='\n'&&files_sel>=0)open_file(files[files_sel].name);}

/* ===== Media Player ===== */
static int media_sel=0;static char media_cur[32]="sakura.wav";static int media_tune=-1;
static int media_list(char out[][32]){int n=0;for(int i=0;i<nfiles&&n<12;i++)if(ends(files[i].name,".wav")||ends(files[i].name,".bmp")||ends(files[i].name,".png"))scpy(out[n++],files[i].name,32);return n;}
static void media_draw(int x,int y,int w,int h){char L[12][32];int n=media_list(L);rect(x,y,190,h,C_SAGE_L);text(x+12,y+10,T("Library","ライブラリ"),C_SAGE_D);
 for(int i=0;i<n;i++){int ry=y+36+i*28;if(seq(L[i],media_cur)&&media_tune<0)rrect(x+6,ry-4,178,26,6,C_WIN);drawch(x+14,ry,ends(L[i],".wav")?0x25B6:0x25A0,C_ROSE_D,1);text(x+36,ry,L[i],C_TEXT);if(hit(x+6,ry-4,178,26)){scpy(media_cur,L[i],32);media_tune=-1;}}
 int ty=y+46+n*28;text(x+12,ty,T("Tunes (speaker)","メロディー"),C_SAGE_D);
 const char*tn[2]={T("Sakura Sakura","さくらさくら"),T("Happy Birthday","ハッピーバースデー")};
 for(int i=0;i<2;i++){int ry=ty+26+i*28;if(media_tune==i)rrect(x+6,ry-4,178,26,6,C_WIN);drawch(x+14,ry,0x25B6,C_SAGE_D,1);text(x+36,ry,tn[i],C_TEXT);if(hit(x+6,ry-4,178,26))media_tune=i;}
 int px0=x+200,pw=w-210;
 if(media_tune>=0){textc(px0,y+60,pw,tn[media_tune],C_TEXT);textc(px0,y+86,pw,T("Plays on the PC speaker","PCスピーカーで再生"),C_SOFT);
  if(button(px0+pw/2-110,y+130,100,34,T("Play","再生"),1))tune_play(media_tune?tune_happy:tune_sakura);
  if(button(px0+pw/2+10,y+130,100,34,T("Stop","停止"),0))tune_stop();
  if(tune){rect(px0+30,y+190,pw-60,6,C_LINE);int tot=0,i2=0;while(tune==tune_sakura?tune_sakura[i2*2+1]:tune_happy[i2*2+1])i2++;tot=i2;rect(px0+30,y+190,(pw-60)*tune_i/(tot?tot:1),6,C_ROSE);}
  return;}
 File*f=fs_find(media_cur);if(!f)return;
 if(ends(media_cur,".bmp")||ends(media_cur,".png")){int iw,ih;if(bmp_size(f->data,&iw,&ih)){int sc=1;while((iw*(sc+1))<=pw-20&&(ih*(sc+1))<=h-50)sc++;bmp_draw(f->data,px0+(pw-iw*sc)/2,y+10,sc);char s[32];itoa(iw,s);scpy(s+slen(s)," x ",4);itoa(ih,s+slen(s));textc(px0,y+h-30,pw,s,C_SOFT);}return;}
 rrect(px0+pw/2-60,y+30,120,120,20,C_ROSE_L);drawch(px0+pw/2-24,y+66,0x25B6,C_ROSE_D,3);textc(px0,y+168,pw,media_cur,C_TEXT);
 if(button(px0+pw/2-110,y+200,100,34,T("Play","再生"),1)){if(!wav_play(f->data,f->size))tune_play(tune_sakura);}
 if(button(px0+pw/2+10,y+200,100,34,T("Stop","停止"),0)){wav_stop();tune_stop();}
 rect(px0+30,y+252,pw-60,6,C_LINE);if(sb_playing){u32 e=ms_now-sb_start;if(e>sb_len)sb_playing=0;else rect(px0+30,y+252,(pw-60)*e/sb_len,6,C_ROSE);}
 textc(px0,y+270,pw,sb_ok?T("Sound card: Sound Blaster 16","サウンド: Sound Blaster 16"):T("No sound card found: using the speaker","サウンドカードなし:スピーカーを使用"),C_SOFT);}
static void media_key(int k){char L[12][32];int n=media_list(L);if(k==K_DOWN||k=='n'){media_sel=(media_sel+1)%n;scpy(media_cur,L[media_sel],32);media_tune=-1;}if(k=='t')media_tune=0;
 if(k==' '){File*f=fs_find(media_cur);if(f&&ends(media_cur,".wav")&&!wav_play(f->data,f->size))tune_play(tune_sakura);}}

/* ===== Paint ===== */
#define PW 520
#define PH 330
static u32 canvas[PW*PH];static int paint_init,pcol=0,psz=4,plx=-1,ply;
static const u32 pal[]={0x4A4C48,0xC9A3A8,0x9DAE98,0xB9B39A,0xA9A6C0,0x9FB8C2,0xD9B99B,0xFFFFFF};
static int paint_cx,paint_cy;
static void paint_draw(int x,int y,int w,int h){(void)w;(void)h;if(!paint_init){for(int i=0;i<PW*PH;i++)canvas[i]=0xFFFFFF;paint_init=1;}
 for(int i=0;i<8;i++){int bx=x+10+i*34;rrect(bx,y+8,28,28,6,pal[i]);if(pal[i]==0xFFFFFF)frame(bx,y+8,28,28,C_LINE);if(i==pcol)frame(bx-2,y+6,32,32,C_ROSE_D);if(hit(bx,y+8,28,28))pcol=i;}
 const int sz[]={2,4,8,16};for(int i=0;i<4;i++){int bx=x+290+i*34;rrect(bx,y+8,28,28,6,psz==sz[i]?C_ROSE_L:C_SAGE_L);rrect(bx+14-sz[i]/2,y+22-sz[i]/2,sz[i],sz[i],sz[i]/2,C_TEXT);if(hit(bx,y+8,28,28))psz=sz[i];}
 if(button(x+430,y+8,50,28,T("Clear","消す"),0))for(int i=0;i<PW*PH;i++)canvas[i]=0xFFFFFF;
 if(button(x+486,y+8,50,28,T("Save","保存"),1)){static u8 b[54+PW*PH*3];memset(b,0,54);b[0]='B';b[1]='M';u32 s=54+PW*PH*3;b[2]=s;b[3]=s>>8;b[4]=s>>16;b[10]=54;b[14]=40;b[18]=PW&255;b[19]=PW>>8;b[22]=PH&255;b[23]=PH>>8;b[26]=1;b[28]=24;
   for(int j=0;j<PH;j++)for(int i=0;i<PW;i++){u32 c=canvas[(PH-1-j)*PW+i];u8*p=b+54+(j*PW+i)*3;p[0]=c;p[1]=c>>8;p[2]=c>>16;}fs_write("painting.bmp",b,s);}
 paint_cx=x+10;paint_cy=y+46;for(int j=0;j<PH;j++)for(int i=0;i<PW;i++)px(paint_cx+i,paint_cy+j,canvas[j*PW+i]);frame(paint_cx-1,paint_cy-1,PW+2,PH+2,C_LINE);}
static void cdot(int x,int y){for(int j=-psz/2;j<psz-psz/2;j++)for(int i=-psz/2;i<psz-psz/2;i++){int a=x+i,b=y+j;if(a>=0&&b>=0&&a<PW&&b<PH)canvas[b*PW+a]=pal[pcol];}}
static void paint_drag(int ev,int x,int y){x-=paint_cx;y-=paint_cy;if(ev==2){plx=-1;return;}if(x<0||y<0||x>=PW||y>=PH){plx=-1;return;}
 if(plx<0){plx=x;ply=y;}int dx=x-plx,dy=y-ply,n=(dx<0?-dx:dx)>(dy<0?-dy:dy)?(dx<0?-dx:dx):(dy<0?-dy:dy);if(!n)n=1;for(int i=0;i<=n;i++)cdot(plx+dx*i/n,ply+dy*i/n);plx=x;ply=y;}

/* ===== Settings ===== */
static void set_draw(int x,int y,int w,int h){(void)h;text(x+20,y+16,T("Language","言語"),C_SAGE_D);
 if(button(x+20,y+40,120,32,"English",lang==0))lang=0;if(button(x+150,y+40,120,32,"日本語",lang==1))lang=1;
 text(x+20,y+92,T("Desktop colour","デスクトップの色"),C_SAGE_D);
 const char*wn[3]={T("Sage mist","セージ"),T("Dusty rose","ローズ"),T("Linen","リネン")};
 for(int i=0;i<3;i++){int bx=x+20+i*130;rrect(bx,y+116,120,50,8,wall_cols[i]);frame(bx,y+116,120,50,i==wall?C_ROSE_D:C_LINE);textc(bx,y+132,120,wn[i],C_TEXT);if(hit(bx,y+116,120,50))wall=i;}
 text(x+20,y+186,T("Clock","時計"),C_SAGE_D);
 if(button(x+20,y+210,200,32,T("Computer clock","パソコンの時計"),!clock_jst))clock_jst=0;if(button(x+230,y+210,200,32,T("Japan time (UTC+9)","日本時間 (UTC+9)"),clock_jst))clock_jst=1;
 y+=70;rect(x+20,y+186,w-40,1,C_LINE);text(x+20,y+200,T("About","この機能について"),C_SAGE_D);
 text(x+20,y+224,T("Petal OS 1.0 - built from scratch for Mat","はなびらOS 1.0 - マットさんのために一から作成"),C_TEXT);
 char m[48];itoa(nfiles,m);scpy(m+slen(m),T(" files in memory"," 個のファイル"),40);text(x+20,y+246,m,C_SOFT);
 text(x+20,y+268,sb_ok?T("Sound: Sound Blaster 16 + speaker","音: Sound Blaster 16 + スピーカー"):T("Sound: PC speaker","音: PCスピーカー"),C_SOFT);
 char r[32];itoa(SW,r);scpy(r+slen(r)," x ",4);itoa(SH,r+slen(r));text(x+20,y+290,T("Screen: ","画面: "),C_SOFT);text(x+20+textw(T("Screen: ","画面: ")),y+290,r,C_SOFT);
 text(x+20,y+312,T("Japanese typing is not ready yet.","日本語入力はまだ使えません。"),C_SOFT);}
static void set_key(int k){if(k=='l')lang=!lang;}

/* ===== Snake ===== */
#define GW 26
#define GH 18
static int sn_x[GW*GH],sn_y[GW*GH],sn_len,sn_dx,sn_dy,sn_fx,sn_fy,sn_dead=1,sn_score,sn_best,sn_started;static u32 sn_t;
static void sn_food(void){for(;;){sn_fx=rnd()%GW;sn_fy=rnd()%GH;int ok=1;for(int i=0;i<sn_len;i++)if(sn_x[i]==sn_fx&&sn_y[i]==sn_fy)ok=0;if(ok)return;}}
static void sn_reset(void){sn_len=4;for(int i=0;i<4;i++){sn_x[i]=8-i;sn_y[i]=9;}sn_dx=1;sn_dy=0;sn_dead=0;sn_score=0;sn_food();sn_started=1;}
static void sn_tick(void){if(sn_dead||!wins[A_SNAKE].open||front()!=A_SNAKE)return;if(ms_now-sn_t<130)return;sn_t=ms_now;int nx=sn_x[0]+sn_dx,ny=sn_y[0]+sn_dy;
 if(nx<0||ny<0||nx>=GW||ny>=GH){sn_dead=1;return;}for(int i=0;i<sn_len;i++)if(sn_x[i]==nx&&sn_y[i]==ny){sn_dead=1;return;}
 int grow=nx==sn_fx&&ny==sn_fy;if(grow&&sn_len<GW*GH)sn_len++;for(int i=sn_len-1;i>0;i--){sn_x[i]=sn_x[i-1];sn_y[i]=sn_y[i-1];}sn_x[0]=nx;sn_y[0]=ny;if(grow){sn_score++;if(sn_score>sn_best)sn_best=sn_score;sn_food();}}
static void snake_draw(int x,int y,int w,int h){(void)h;char s[48];scpy(s,T("Score ","スコア "),16);itoa(sn_score,s+slen(s));text(x+12,y+8,s,C_TEXT);scpy(s,T("Best ","ベスト "),16);itoa(sn_best,s+slen(s));text(x+w-textw(s)-12,y+8,s,C_SOFT);
 int gx=x+12,gy=y+32;rrect(gx-2,gy-2,GW*18+4,GH*18+4,6,C_SAGE_L);rrect(gx+sn_fx*18+2,gy+sn_fy*18+2,14,14,7,C_ROSE);
 for(int i=0;i<sn_len;i++)rrect(gx+sn_x[i]*18+1,gy+sn_y[i]*18+1,16,16,4,i?C_SAGE:C_SAGE_D);
 if(sn_dead){rrect(gx+GW*9-150,gy+GH*9-40,300,80,10,C_WIN);textc(gx+GW*9-150,gy+GH*9-30,300,sn_started?T("Game over!","ゲームオーバー!"):T("Snake","ヘビ"),C_ROSE_D);textc(gx+GW*9-150,gy+GH*9,300,T("Press Space to play","スペースキーでスタート"),C_SOFT);if(hit(gx,gy,GW*18,GH*18))sn_reset();}}
static void snake_key(int k){if(k==' '&&sn_dead)sn_reset();if(k==K_UP&&sn_dy!=1){sn_dx=0;sn_dy=-1;}if(k==K_DOWN&&sn_dy!=-1){sn_dx=0;sn_dy=1;}if(k==K_LEFT&&sn_dx!=1){sn_dx=-1;sn_dy=0;}if(k==K_RIGHT&&sn_dx!=-1){sn_dx=1;sn_dy=0;}}

/* ===== Noughts and Crosses ===== */
static int tb[9],t_over,t_wins,t_losses;
static int t_line(int p){static int L[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};for(int i=0;i<8;i++)if(tb[L[i][0]]==p&&tb[L[i][1]]==p&&tb[L[i][2]]==p)return 1;return 0;}
static int t_full(void){for(int i=0;i<9;i++)if(!tb[i])return 0;return 1;}
static void t_ai(void){for(int p=2;p>=1;p--)for(int i=0;i<9;i++)if(!tb[i]){tb[i]=p;if(t_line(p)){tb[i]=2;return;}tb[i]=0;}if(!tb[4]){tb[4]=2;return;}
 int c[]={0,2,6,8,1,3,5,7};int st=rnd()%4;for(int k=0;k<8;k++){int i=c[k<4?(k+st)%4:k];if(!tb[i]){tb[i]=2;return;}}}
static void ttt_draw(int x,int y,int w,int h){(void)h;int s=96,gx=x+(w-s*3)/2,gy=y+40;
 const char*msg=t_over==1?T("You win!","あなたの勝ち!"):t_over==2?T("I win!","わたしの勝ち!"):t_over==3?T("A draw.","引き分け"):T("You are X. Click a square.","あなたはX。マスをクリック");textc(x,y+12,w,msg,t_over?C_ROSE_D:C_TEXT);
 for(int i=0;i<9;i++){int bx=gx+(i%3)*s,by=gy+(i/3)*s;rrect(bx+3,by+3,s-6,s-6,10,C_SAGE_L);
  if(tb[i]==1){line(bx+24,by+24,bx+s-24,by+s-24,6,C_ROSE_D);line(bx+s-24,by+24,bx+24,by+s-24,6,C_ROSE_D);}
  if(tb[i]==2){for(int a=0;a<360;a+=3){static const short sn[]={0,52,105,156,208,259,309,358,407,454,500,545,588,629,669,707,743,777,809,839,866,891,914,934,951,966,978,988,995,999,1000};}
   for(int yy=-30;yy<=30;yy++)for(int xx=-30;xx<=30;xx++){int r=xx*xx+yy*yy;if(r<=900&&r>=24*24)px(bx+s/2+xx,by+s/2+yy,C_SAGE_D);}}
  if(!t_over&&!tb[i]&&hit(bx,by,s,s)){tb[i]=1;if(t_line(1)){t_over=1;t_wins++;}else if(t_full())t_over=3;else{t_ai();if(t_line(2)){t_over=2;t_losses++;}else if(t_full())t_over=3;}}}
 char sc[48];scpy(sc,T("You ","あなた "),16);itoa(t_wins,sc+slen(sc));scpy(sc+slen(sc)," : ",4);itoa(t_losses,sc+slen(sc));scpy(sc+slen(sc),T(" Me"," わたし"),16);text(x+14,gy+s*3+14,sc,C_SOFT);
 if(button(x+w-130,gy+s*3+8,116,30,T("New game","もう一回"),0)){memset(tb,0,sizeof tb);t_over=0;}}
static void ttt_key(int k){if(k==' '||k=='\n'){memset(tb,0,sizeof tb);t_over=0;}}

App apps[NAPP]={
 {"Files","ファイル","F",520,420,0x9DAE98,files_draw,files_key,0},
 {"Notes","メモ","N",560,420,0xB9B39A,notes_draw,notes_key,0},
 {"Calculator","電卓","+",300,360,0xA9A6C0,calc_draw,calc_key,0},
 {"Calendar","カレンダー","31",400,420,0xC9A3A8,cal_draw,cal_key,0},
 {"Media Player","メディア",">",620,400,0x9FB8C2,media_draw,media_key,0},
 {"Paint","お絵かき","P",540,386,0xD9B99B,paint_draw,0,paint_drag},
 {"Browser","ブラウザ","W",600,480,0x8FA9A0,web_draw,web_key,0},
 {"Settings","設定","S",480,410,0xA8A49E,set_draw,set_key,0},
 {"Snake","ヘビ","~",496,380,0x9DAE98,snake_draw,snake_key,0},
 {"Noughts & Crosses","まるばつ","X",360,390,0xC9A3A8,ttt_draw,ttt_key,0},
};
static void open_file(const char*n){if(ends(n,".txt")){notes_load(n);open_app(A_NOTES);}else if(ends(n,".html")){web_go(n);open_app(A_WEB);}else{scpy(media_cur,n,32);media_tune=-1;open_app(A_MEDIA);}}

/* ---------- desktop ---------- */
static int rs_win=-1,rs_x0,rs_y0,rs_w0,rs_h0;static int menu_open,drag_win=-1,drag_dx,drag_dy,app_drag=-1;
static int pend_click,pend_x,pend_y; /* click routed into next draw */
static int pend_app=-1;
static void icon_pos(int i,int*x,int*y){*x=10+(i/6)*100;*y=14+(i%6)*100;}
static const u16 icon_art[NAPP][16]={{0x0000,0x0000,0x3E00,0x7F00,0x7FFE,0x7FFE,0x7FFE,0x7FFE,0x7FFE,0x7FFE,0x7FFE,0x7FFE,0x0000,0x0000,0x0000,0x0000},
{0x0000,0x1FF8,0x1008,0x1008,0x17E8,0x1008,0x17E8,0x1008,0x17E8,0x1008,0x1788,0x1008,0x1FF8,0x0000,0x0000,0x0000},
{0x0000,0x1FF8,0x1008,0x17E8,0x17E8,0x1008,0x16D8,0x16D8,0x1008,0x16D8,0x16D8,0x1008,0x1FF8,0x0000,0x0000,0x0000},
{0x0000,0x0840,0x3FFC,0x3FFC,0x3FFC,0x2004,0x2DB4,0x2004,0x2DB4,0x2004,0x2D84,0x2004,0x3FFC,0x0000,0x0000,0x0000},
{0x0000,0x0000,0x0C00,0x0F00,0x0FC0,0x0FF0,0x0FFC,0x0FFE,0x0FFC,0x0FF0,0x0FC0,0x0F00,0x0C00,0x0000,0x0000,0x0000},
{0x0000,0x000E,0x001E,0x003C,0x0078,0x00F0,0x01E0,0x03C0,0x0B80,0x1E00,0x3E00,0x3C00,0x7800,0x6000,0x0000,0x0000},
{0x0000,0x07E0,0x1998,0x2244,0x4242,0x7FFE,0x4422,0x4422,0x7FFE,0x4242,0x2244,0x1998,0x07E0,0x0000,0x0000,0x0000},
{0x0000,0x0180,0x1998,0x1FF8,0x0E70,0x1C38,0x781E,0x781E,0x1C38,0x0E70,0x1FF8,0x1998,0x0180,0x0000,0x0000,0x0000},
{0x0000,0x0000,0x1FC0,0x3FE0,0x3060,0x3000,0x3FC0,0x1FE0,0x0030,0x3030,0x3FF0,0x1FEC,0x000C,0x0000,0x0000,0x0000},
{0x0000,0x0C60,0x4D66,0x2CB6,0x4D66,0xFFFC,0x0C60,0x0DB0,0x0D30,0x0DB0,0xFFFC,0x0C60,0x6D64,0x6CE8,0x0C60,0x0000}};
static void draw_icon(int x,int y,int a,int big){int s=big?48:20;rrect(x,y,s,s,big?12:5,apps[a].col);int cs=big?2:1,o=big?8:2;
 for(int j=0;j<16;j++){u16 r=icon_art[a][j];for(int i=0;i<16;i++)if(r&(0x8000>>i))rect(x+o+i*cs,y+o+j*cs+(big?1:0),cs,cs,0xFFFFFF);}}
static void draw_window(int a){Win*w=&wins[a];int W=w->w,H=w->h,act=front()==a;clip_all();
 rrect(w->x+3,w->y+4,W,H+TB,10,mix(wall_cols[wall],0x000000,28));rrect(w->x,w->y,W,H+TB,10,act?C_ROSE:0xD6D3CE);rect(w->x+1,w->y+TB,W-2,H-8,C_WIN);rrect(w->x+1,w->y+TB+H-20,W-2,19,9,C_WIN);
 draw_icon(w->x+8,w->y+4,a,0);text(w->x+36,w->y+6,T(apps[a].en,apps[a].ja),act?0xFFFFFF:C_TEXT);
 rrect(w->x+W-26,w->y+5,18,18,9,act?C_ROSE_D:0xC4C1BC);drawch(w->x+W-21,w->y+6,0xD7,0xFFFFFF,1);
 clicked=0;if(pend_click&&pend_app==a){clicked=1;clickx=pend_x;clicky=pend_y;pend_click=0;}
 clip(w->x+1,w->y+TB,W-2,H-2);apps[a].draw(w->x+1,w->y+TB,W-2,H-2);clip_all();
 for(int i=0;i<3;i++)for(int j=0;j<=i;j++)rect(w->x+W-6-j*4,w->y+TB+H-6-(i-j)*4,2,2,0xB8B4AE);}
static void draw_cursor(void){static const char*c[]={"X","XX","X.X","X..X","X...X","X....X","X.....X","X......X","X.......X","X........X","X.....XXXX","X..X..X","X.X X..X","XX  X..X","    X..X","     XX"};
 for(int j=0;j<16;j++)for(int i=0;c[j][i];i++){if(c[j][i]=='X')px(mx+i,my+j,0x3E403C);else if(c[j][i]=='.')px(mx+i,my+j,0xFFFFFF);}}
static void render(void){clip_all();rect(0,0,SW,SH,wall_cols[wall]);
 /* soft petal decoration */
 for(int k=0;k<5;k++){int ox=SW-200+(k%2)*40-k*14,oy=SH-260+k*20;(void)ox;(void)oy;}
 for(int yy=-70;yy<=70;yy++)for(int xx=-70;xx<=70;xx++){int r=xx*xx+yy*yy;if(r<4900)px(SW-170+xx,SH-200+yy,mix(wall_cols[wall],C_ROSE,r<1200?60:30));}
 text(SW-260,SH-110,T("Petal OS","はなびらOS"),mix(wall_cols[wall],C_TEXT,120));
 for(int a=0;a<NAPP;a++){int x,y;icon_pos(a,&x,&y);draw_icon(x+14+(76-76)/2+0,y,a,1);const char*n=T(apps[a].en,apps[a].ja);int sp=-1;if(textw(n)>96)for(int q=0;n[q];q++)if(n[q]==' '){sp=q;break;}
  if(sp<0)textc(x-10,y+54,96,n,C_TEXT);else{int w1=textwn(n,sp,1);textn(x+38-w1/2,y+54,n,sp,C_TEXT,1);textc(x-10,y+72,96,n+sp+1,C_TEXT);}}
 for(int i=0;i<nz;i++)draw_window(zorder[i]);pend_click=0;
 /* taskbar */
 int ty=SH-40;rect(0,ty,SW,40,0xF0EEEA);rect(0,ty,SW,1,C_LINE);
 rrect(6,ty+6,100,28,8,menu_open?C_ROSE_D:C_ROSE);drawch(14,ty+12,0x2022,0xFFFFFF,1);text(30,ty+12,T("Petal","はなびら"),0xFFFFFF);
 int bx=116;for(int i=0;i<NAPP;i++)if(wins[i].open){int bw=130;rrect(bx,ty+6,bw,28,8,front()==i?C_SAGE_L:0xE6E4DF);draw_icon(bx+6,ty+10,i,0);clip(bx,ty,bw-6,40);text(bx+32,ty+12,T(apps[i].en,apps[i].ja),C_TEXT);clip_all();bx+=bw+6;}
 rtc_read();char t[8];itoa2(rtc_h,t);t[2]=':';itoa2(rtc_m,t+3);text(SW-60,ty+12,t,C_TEXT);
 char d[16];itoa(rtc_y,d);d[4]='/';itoa2(rtc_mo,d+5);d[7]='/';itoa2(rtc_d,d+8);text(SW-160,ty+12,d,C_SOFT);
 if(menu_open){int mh=NAPP*32+16;int my0=ty-mh-4;rrect(6,my0,240,mh,10,C_WIN);frame(6,my0,240,mh,C_LINE);
  for(int a=0;a<NAPP;a++){int ry=my0+8+a*32;if(mx>=6&&mx<246&&my>=ry&&my<ry+32)rrect(10,ry,232,30,6,C_ROSE_L);draw_icon(18,ry+5,a,0);text(48,ry+7,T(apps[a].en,apps[a].ja),C_TEXT);}}
 draw_cursor();
 for(u32 j=0;j<SH;j++)memcpy((u8*)fb+j*pitch,back+j*SW,SW*4);}
static void on_mouse(void){int press=mbtn&&!mprev,rel=!mbtn&&mprev;mprev=mbtn;int ty=SH-40;
 if(rs_win>=0){if(mbtn){Win*w=&wins[rs_win];w->w=rs_w0+mx-rs_x0;w->h=rs_h0+my-rs_y0;if(w->w<260)w->w=260;if(w->h<160)w->h=160;if(w->w>(int)SW-w->x)w->w=SW-w->x;if(w->h>(int)SH-40-TB-w->y)w->h=SH-40-TB-w->y;}else rs_win=-1;return;}
 if(drag_win>=0){if(mbtn){wins[drag_win].x=mx-drag_dx;wins[drag_win].y=my-drag_dy;if(wins[drag_win].y<0)wins[drag_win].y=0;}else drag_win=-1;return;}
 if(app_drag>=0){if(apps[app_drag].drag)apps[app_drag].drag(mbtn?1:2,mx,my);if(!mbtn)app_drag=-1;return;}
 (void)rel;if(!press)return;
 if(menu_open){int mh=NAPP*32+16,my0=ty-mh-4;menu_open=0;if(mx>=6&&mx<246&&my>=my0+8&&my<my0+8+NAPP*32){open_app((my-my0-8)/32);return;}if(my>=ty&&mx<106)return;}
 if(my>=ty){if(mx<106){menu_open=1;return;}int bx=116;for(int i=0;i<NAPP;i++)if(wins[i].open){if(mx>=bx&&mx<bx+130){if(front()==i)close_app(i),open_app(i);else focus(i);return;}bx+=136;}return;}
 for(int z=nz-1;z>=0;z--){int a=zorder[z];Win*w=&wins[a];int W=w->w,H=w->h;if(mx>=w->x&&mx<w->x+W&&my>=w->y&&my<w->y+H+TB){focus(a);
   if(my<w->y+TB){if(mx>=w->x+W-28){close_app(a);return;}drag_win=a;drag_dx=mx-w->x;drag_dy=my-w->y;return;}
   if(mx>=w->x+W-16&&my>=w->y+TB+H-16){rs_win=a;rs_x0=mx;rs_y0=my;rs_w0=W;rs_h0=H;return;}
   pend_click=1;pend_x=mx;pend_y=my;pend_app=a;if(apps[a].drag){app_drag=a;apps[a].drag(0,mx,my);}return;}}
 for(int a=0;a<NAPP;a++){int x,y;icon_pos(a,&x,&y);if(mx>=x&&mx<x+80&&my>=y&&my<y+90){open_app(a);return;}}}
static void on_key(int k){if(k>=K_F1&&k<K_F1+NAPP){open_app(k-K_F1);return;}
 if(k==27){if(menu_open)menu_open=0;else if(front()>=0)close_app(front());return;}
 if(k=='\t'&&nz>1){focus(zorder[0]);return;}
 if(k==K_F1+11){menu_open=!menu_open;return;}
 int f=front();if(f>=0&&apps[f].key)apps[f].key(k);}

/* ---------- boot ---------- */
static void panic_text(const char*s){(void)s;for(;;)__asm__("hlt");}
void kmain(u32 magic,u32*mbi){if(magic!=0x2BADB002||!(mbi[0]&(1<<12)))panic_text("no framebuffer");
 fb=(u32*)(u32)*(u64*)(mbi+22);pitch=mbi[24];SW=mbi[25];SH=mbi[26];if(SW>1280)SW=1280;if(SH>1024)SH=1024;
 pit_init();rtc_read();rng^=rtc_s*7919+rtc_m*31;fs_init();sb_init();mouse_init();mx=SW/2;my=SH/2;
 notes_load("welcome.txt");
 u32 last=0;for(;;){pit_poll();input_poll();tune_poll();sn_tick();if(sb_playing&&ms_now-sb_start>sb_len)sb_playing=0;
  if(ms_now-last>=33||pend_click){last=ms_now;render();}}}
