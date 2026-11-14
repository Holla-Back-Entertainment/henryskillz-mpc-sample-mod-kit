#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
/* Console EQ: eq_spectrum_draw renders the response curve into a w x h buffer;
   we overlay a dark Hz/dB grid (mixer-style) that only paints over background
   pixels, so the curve + axis stay crisp. The orange response curve is drawn in
   a separate later pass (not in this buffer), so it stays its readable amber. */
typedef void (*eqs_fn)(uint32_t*,int,int,int);
static eqs_fn real_eqs;
static const uint32_t GRID=0x00222A31u, ZERO=0x004E565Du, FRAME=0x0030383Eu;
static inline int bg(uint32_t p){unsigned r=(p>>16)&255,g=(p>>8)&255,b=p&255;return (r+g+b)<70;}
static void vln(uint32_t*p,int w,int h,int x,uint32_t c){if(x<0||x>=w)return;for(int y=0;y<h;++y){uint32_t*q=&p[y*w+x];if(c==FRAME||bg(*q))*q=c;}}
static void hln(uint32_t*p,int w,int h,int y,uint32_t c){if(y<0||y>=h)return;for(int x=0;x<w;++x){uint32_t*q=&p[y*w+x];if(c==FRAME||bg(*q))*q=c;}}
void eq_spectrum_draw(uint32_t*buf,int w,int h,int a3){
  if(!real_eqs)real_eqs=(eqs_fn)dlsym(RTLD_NEXT,"eq_spectrum_draw");
  if(real_eqs)real_eqs(buf,w,h,a3);
  if(!buf||w<=0||h<=0||w>1024||h>1024)return;
  for(int k=1;k<10;++k)vln(buf,w,h,(int)((long)w*k/10),GRID);  /* Hz grid */
  for(int k=1;k<8;++k){int y=(int)((long)h*k/8);hln(buf,w,h,y,k==4?ZERO:GRID);} /* dB grid, 0dB brighter */
  hln(buf,w,h,0,FRAME);hln(buf,w,h,h-1,FRAME);vln(buf,w,h,0,FRAME);vln(buf,w,h,w-1,FRAME);
}

/* Parametric EQ pages: the pad EQ page (Pad Mixer, B3/ENCODER on the row EQ) and the eq_ui page
   never call eq_spectrum_draw. They draw the whole page with eq_draw(buf, w, h, y0, top, knobs,
   focus, bypass): buf holds screen rows y0..y0+h-1, cleared to black. In SampQOL 0.2.0 the graph is
   screen x 15..302 (288 px, 30 Hz..18 kHz) and screen y top+1 (+6 dB) .. 187 (-36 dB), with stock's own
   0 dB line at 1/7. Same grid look as above, clipped to that rect: 9 Hz lines, a dB line every 6 dB,
   frame included, all background-gated, so the curve, stock 0 dB line, band markers and labels are never painted over.
   Callers: 0x76018 (eq_ui, 320x164 at y0 76) and 0xab798/0xab7c4 (pad EQ, 320x160 at y0 40 + the
   320x40 soft-key bar at y0 200, which misses the graph). The geometry is 0.2.0's, so on any other
   build (eq_draw not at 0x73710 in mpcqol.so) every call goes straight through. */
#define QOL_EQ_DRAW 0x73710
typedef void (*eqd_fn)(uint32_t*,int,int,int,int,const float*,int,int);
static eqd_fn real_eqd;
static int eqd_matched=-1;           /* -1 not checked yet, 0 other build, 1 SampQOL 0.2.0 */
enum{GX0=15,GX1=302,GBOT=187};
static void gpx(uint32_t*p,int w,int h,int x,int y,uint32_t c){if(x>=0&&x<w&&y>=0&&y<h&&bg(p[y*w+x]))p[y*w+x]=c;}
void eq_draw(uint32_t*buf,int w,int h,int y0,int top,const float*knobs,int focus,int bypass){
  if(eqd_matched<0){
    real_eqd=(eqd_fn)dlsym(RTLD_NEXT,"eq_draw");
    Dl_info in;const char*n;
    eqd_matched=real_eqd&&dladdr((void*)real_eqd,&in)&&in.dli_fname&&
      !strcmp((n=strrchr(in.dli_fname,'/'))?n+1:in.dli_fname,"mpcqol.so")&&
      (uintptr_t)real_eqd-(uintptr_t)in.dli_fbase==QOL_EQ_DRAW;
  }
  if(real_eqd)real_eqd(buf,w,h,y0,top,knobs,focus,bypass);
  if(eqd_matched!=1||!buf||w<=GX1||w>1024||h<=0||h>1024||top<0||top>=GBOT-8)return;
  int t=top+1-y0,b=GBOT-y0,gh=GBOT-top;            /* graph rows in buf; gh = rows from +6 to -36 dB */
  for(int k=1;k<10;++k){int x=GX0+(GX1-GX0)*k/10;for(int y=t;y<=b;++y)gpx(buf,w,h,x,y,GRID);}  /* Hz grid */
  for(int k=1;k<7;++k){int y=t+gh*k/7;for(int x=GX0;x<=GX1;++x)gpx(buf,w,h,x,y,GRID);}         /* 6 dB steps */
  for(int x=GX0-1;x<=GX1+1;++x){gpx(buf,w,h,x,t-1,FRAME);gpx(buf,w,h,x,b+1,FRAME);}                    /* frame */
  for(int y=t-1;y<=b+1;++y){gpx(buf,w,h,GX0-1,y,FRAME);gpx(buf,w,h,GX1+1,y,FRAME);}
}
