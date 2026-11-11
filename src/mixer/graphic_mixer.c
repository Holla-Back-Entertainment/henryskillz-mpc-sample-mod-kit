// HS graphic channel mixer for SampQOL 0.2.0: redraws SampQOL's Ch. Mixer and Pad Mixer (PAD FX, then B2)
// as a console: 8 strips per page with pan knob, mute/solo, fader, stereo meter with peak and the output.
// It reads SampQOL's own mixer snapshot, so it only draws when mixview_frame sits at its 0.2.0 offset
// in mpcqol.so; any other build gets SampQOL's own mixer, untouched.
// Build: see build.sh at the top of the kit (gcc -O2 -fPIC -shared graphic_mixer.c -lm -ldl)
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define QOL_MIXVIEW_FRAME 0xdf230u   /* mixview_frame in mpcqol.so of SampQOL 0.2.0 */
enum { W = 320, H = 256, STRIP_BYTES = 116 };
typedef void (*frame_fn)(int, int, const char *, int, int, void *, void *, void *);
typedef void (*overlay_fn)(int, const void *, int);
static frame_fn original_frame;
static overlay_fn overlay_update_fn;
static uint32_t console_pixels[W * H];

static inline uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }

static void rect(uint32_t *p, int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x; if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) p[yy * W + xx] = c;
}
static void frame(uint32_t *p, int x, int y, int w, int h, uint32_t c)
{
    rect(p,x,y,w,1,c); rect(p,x,y+h-1,w,1,c); rect(p,x,y,1,h,c); rect(p,x+w-1,y,1,h,c);
}

static void pan_knob(uint32_t *p, int cx, int cy, float pan)
{
    if (pan < 0) pan = 0; if (pan > 1) pan = 1;
    /* Nine-pixel rotary control: deliberately chunky enough for the real LCD. */
    rect(p,cx-2,cy-4,5,1,rgb(116,125,131));
    rect(p,cx-3,cy-3,7,1,rgb(116,125,131));
    rect(p,cx-4,cy-2,1,5,rgb(116,125,131)); rect(p,cx+4,cy-2,1,5,rgb(116,125,131));
    rect(p,cx-3,cy+3,7,1,rgb(116,125,131)); rect(p,cx-2,cy+4,5,1,rgb(116,125,131));
    rect(p,cx-2,cy-2,5,5,rgb(22,27,31));
    float a = (-2.3561945f + pan * 4.712389f);
    int ex = cx + (int)lroundf(sinf(a) * 3.0f);
    int ey = cy - (int)lroundf(cosf(a) * 3.0f);
    rect(p,cx,cy,1,1,rgb(67,174,225)); rect(p,ex,ey,1,1,rgb(92,205,255));
}

#define G(a,b,c,d,e) ((a)|((b)<<3)|((c)<<6)|((d)<<9)|((e)<<12))
static uint16_t glyph(char c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    switch (c) {
    case 'A': return G(2,5,7,5,5); case 'B': return G(6,5,6,5,6); case 'C': return G(3,4,4,4,3);
    case 'D': return G(6,5,5,5,6); case 'E': return G(7,4,6,4,7); case 'F': return G(7,4,6,4,4);
    case 'G': return G(3,4,5,5,3); case 'H': return G(5,5,7,5,5); case 'I': return G(7,2,2,2,7);
    case 'J': return G(1,1,1,5,2); case 'K': return G(5,5,6,5,5); case 'L': return G(4,4,4,4,7);
    case 'M': return G(5,7,7,5,5); case 'N': return G(5,7,7,7,5); case 'O': return G(2,5,5,5,2);
    case 'P': return G(6,5,6,4,4); case 'Q': return G(2,5,5,7,3); case 'R': return G(6,5,6,5,5);
    case 'S': return G(3,4,2,1,6); case 'T': return G(7,2,2,2,2); case 'U': return G(5,5,5,5,7);
    case 'V': return G(5,5,5,5,2); case 'W': return G(5,5,7,7,5); case 'X': return G(5,5,2,5,5);
    case 'Y': return G(5,5,2,2,2); case 'Z': return G(7,1,2,4,7); case '0': return G(7,5,5,5,7);
    case '1': return G(2,6,2,2,7); case '2': return G(6,1,7,4,7); case '3': return G(6,1,3,1,6);
    case '4': return G(5,5,7,1,1); case '5': return G(7,4,6,1,6); case '6': return G(3,4,6,5,2);
    case '7': return G(7,1,2,2,2); case '8': return G(2,5,2,5,2); case '9': return G(2,5,3,1,6);
    case '-': return G(0,0,7,0,0); case '.': return G(0,0,0,0,2); case '/': return G(1,1,2,4,4);
    case '>': return G(4,2,1,2,4); case '+': return G(0,2,7,2,0); case ':': return G(0,2,0,2,0);
    default: return 0;
    }
}
static int text_width(const char *s, int max) { int n=0; while(s&&*s&&n<max){++s;++n;} return n?n*4-1:0; }
static void text(uint32_t *p, int x, int y, const char *s, int max, uint32_t c)
{
    for(int n=0;s&&*s&&n<max;++n,++s,x+=4){ uint16_t g=glyph(*s); for(int yy=0;yy<5;++yy){ unsigned row=(g>>(yy*3))&7; for(int xx=0;xx<3;++xx) if(row&(4>>xx)) rect(p,x+xx,y+yy,1,1,c); }}
}
static float f32(const uint8_t *s,int off){ float v; memcpy(&v,s+off,4); return isfinite(v)?v:0.0f; }
static int i32(const uint8_t *s,int off){ int v; memcpy(&v,s+off,4); return v; }
static int meter_y(float v){ if(!(v>.0001f))return 90; float db=10*log10f(v); if(db>6)db=6;if(db<-54)db=-54;return 28+(int)((6-db)*60/60); }
static int fader_y(float v){ if(!(v>.001f))return 87; float db=20*log10f(v);if(db>10)db=10;if(db<-50)db=-50;return 31+(int)((10-db)*56/60); }
static void meter(uint32_t *p,int x,const uint8_t *s,int ch)
{
    int top=meter_y(f32(s,56+ch*4)); rect(p,x,27,3,63,rgb(5,8,8));
    for(int y=87;y>=top;y-=4){ uint32_t c=y<34?rgb(245,55,40):y<43?rgb(242,177,34):rgb(50,205,91);rect(p,x,y,3,3,c); }
    int pk=meter_y(f32(s,64+ch*4)); if(pk<90)rect(p,x,pk,3,1,rgb(255,236,181));
}
static uint32_t strip_colour(const uint8_t *s)
{
    uint32_t bgr;memcpy(&bgr,s+40,4);unsigned r=bgr&255,g=(bgr>>8)&255,b=(bgr>>16)&255;return (r|g|b)?rgb(r,g,b):rgb(205,211,216);
}

static void draw_console(uintptr_t base,int view)
{
    const uint8_t *snap=(const uint8_t *)(base+0x1957ddcu); uint32_t *p=console_pixels;
    int count=i32(snap,4),selected=*(int *)(base+0x1957948u+(uintptr_t)view*4u),editing=*(int *)(base+0x1957930u+(uintptr_t)view*4u);
    int first=selected>=0?(selected/8)*8:0; memset(p,0,sizeof(console_pixels)); rect(p,0,0,W,106,rgb(7,9,11)); rect(p,0,0,W,2,rgb(200,6,28)); /* red brand bar */
    for(int slot=0;slot<8;++slot){
        int idx=first+slot,x=4+slot*39;const uint8_t *s=idx<count?snap+12+idx*STRIP_BYTES:0;int sel=idx==selected&&s;
        rect(p,x,1,36,105,sel?rgb(29,35,40):rgb(18,22,26));frame(p,x,1,36,105,rgb(48,56,62));
        uint32_t accent=s?strip_colour(s):rgb(51,58,64);rect(p,x+1,2,34,2,accent);
        if(s) pan_knob(p,x+18,9,f32(s,52)); else pan_knob(p,x+18,9,.5f);
        int flags=s?i32(s,72):0;rect(p,x+4,16,12,9,(flags&1)?rgb(245,91,30):rgb(61,66,70));rect(p,x+20,16,12,9,(flags&2)?rgb(255,194,25):rgb(61,66,70));
        frame(p,x+4,16,12,9,rgb(104,108,111));frame(p,x+20,16,12,9,rgb(104,108,111));text(p,x+9,18,"M",1,rgb(234,234,228));text(p,x+25,18,"S",1,rgb(234,234,228));
        rect(p,x+11,27,4,63,rgb(7,9,10));rect(p,x+12,28,1,61,rgb(69,74,78));for(int y=32;y<=87;y+=9)rect(p,x+5,y,4,1,rgb(116,121,124));
        if(s){int fy=fader_y(f32(s,48));rect(p,x+6,fy+1,13,5,rgb(79,82,83));rect(p,x+5,fy,15,4,rgb(224,224,216));rect(p,x+5,fy+2,15,1,rgb(126,129,128));meter(p,x+25,s,0);meter(p,x+30,s,1);}
        else{rect(p,x+5,86,15,3,rgb(75,78,79));rect(p,x+25,27,3,63,rgb(5,8,8));rect(p,x+30,27,3,63,rgb(5,8,8));}
        const char *out=s?(const char *)(s+92):""; if(s && !strncmp(out,"Master",6)) out="MSTR";
        text(p,x+(36-text_width(out,5))/2,96,out,5,rgb(148,161,170));
        if(sel){uint32_t c=editing?rgb(255,205,25):rgb(76,184,231);frame(p,x-1,0,38,106,c);if(editing)frame(p,x+1,2,34,102,rgb(145,111,9));}
    }
    if(!overlay_update_fn)overlay_update_fn=(overlay_fn)dlsym(RTLD_NEXT,"overlay_update");if(overlay_update_fn)overlay_update_fn(33,p,1);
}

__attribute__((visibility("default")))
void mixview_frame(int view,int mode,const char *label,int arg3,int arg4,void *arg5,void *arg6,void *arg7)
{
    static int matched = -1;    /* -1 not checked yet, 0 other build, 1 SampQOL 0.2.0 */
    if(!original_frame)original_frame=(frame_fn)dlsym(RTLD_NEXT,"mixview_frame");if(!original_frame)return;
    if(matched<0){Dl_info info;matched=dladdr((void *)original_frame,&info)&&(uintptr_t)original_frame-(uintptr_t)info.dli_fbase==QOL_MIXVIEW_FRAME;}
    original_frame(view,mode,label,arg3,arg4,arg5,arg6,arg7);if(matched)draw_console((uintptr_t)original_frame-QOL_MIXVIEW_FRAME,view);
}
