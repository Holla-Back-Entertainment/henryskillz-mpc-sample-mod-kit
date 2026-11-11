#ifndef HS_CONSOLE_H
#define HS_CONSOLE_H

#include <math.h>
#include <stdint.h>

/* Define W and H to the dimensions of the drawing target before including. */
static inline uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }

static inline void rect(uint32_t *p, int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) p[yy * W + xx] = c;
}
static inline void frame(uint32_t *p, int x, int y, int w, int h, uint32_t c)
{
    rect(p,x,y,w,1,c); rect(p,x,y+h-1,w,1,c); rect(p,x,y,1,h,c); rect(p,x+w-1,y,1,h,c);
}

static inline void pan_knob(uint32_t *p, int cx, int cy, float pan)
{
    if (pan < 0) pan = 0;
    if (pan > 1) pan = 1;
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
static inline uint16_t glyph(char c)
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
    case '%': return G(5,1,2,4,5);
    default: return 0;
    }
}
static inline int text_width(const char *s, int max) { int n=0; while(s&&*s&&n<max){++s;++n;} return n?n*4-1:0; }
static inline void text(uint32_t *p, int x, int y, const char *s, int max, uint32_t c)
{
    for(int n=0;s&&*s&&n<max;++n,++s,x+=4){ uint16_t g=glyph(*s); for(int yy=0;yy<5;++yy){ unsigned row=(g>>(yy*3))&7; for(int xx=0;xx<3;++xx) if(row&(4>>xx)) rect(p,x+xx,y+yy,1,1,c); }}
}
#endif
