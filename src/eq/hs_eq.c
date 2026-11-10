#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
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
