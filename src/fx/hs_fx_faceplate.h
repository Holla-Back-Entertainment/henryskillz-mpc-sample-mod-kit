#ifndef HS_FX_FACEPLATE_H
#define HS_FX_FACEPLATE_H

#include "hs_flavor_art.h"

static void flavor_sprite(uint32_t *p, int x, int y, int width, int height, const uint32_t *pixels)
{
    for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col) {
            uint32_t source = pixels[row * width + col];
            unsigned alpha = source >> 24;
            uint32_t *dest = p + (y + row) * W + x + col;
            if (alpha == 255) {
                *dest = source & 0xffffff;
            } else if (alpha) {
                unsigned inverse = 255 - alpha;
                unsigned r = (((source >> 16) & 255) * alpha + ((*dest >> 16) & 255) * inverse) / 255;
                unsigned g = (((source >> 8) & 255) * alpha + ((*dest >> 8) & 255) * inverse) / 255;
                unsigned b = ((source & 255) * alpha + (*dest & 255) * inverse) / 255;
                *dest = rgb(r, g, b);
            }
        }
}

static void flavor_control(uint32_t *p, int x, int value, int power_module, int bypassed)
{
    static const uint32_t *const switches[] = {
        flavor_switch_on_pitch, flavor_switch_on_distortion, flavor_switch_on_digital,
        flavor_switch_on_vinyl, flavor_switch_on_volume, flavor_switch_on_timbre
    };
    if (power_module >= 0 && power_module < 6) {
        const uint32_t *sprite = value >= 37 && !bypassed
            ? switches[power_module] : flavor_switch_off;
        flavor_sprite(p, x - 12, 96, 25, 38, sprite);
    } else {
        int frame = value >= 0 ? (value * 24 + 36) / 73 : 0;
        flavor_sprite(p, x - 21, 94, 43, 43, flavor_knobs + frame * 43 * 43);
    }
}

struct faceplate {
    uint32_t metal;
    uint32_t accent;
    const char *legend;
};

static const struct faceplate faceplates[] = {
    [DELAY] = {0x202a30, 0x7faeba, "DIGITAL REPEATS"},
    [DIFF_DELAY] = {0x242731, 0x8ea6cc, "STEREO REPEATS"},
    [TAPE_DELAY] = {0x322b23, 0xd59a55, "TAPE ECHO"},
    [SAMPLE_DELAY] = {0x252c30, 0x88b9ac, "SAMPLE ALIGNMENT"},
    [REVERB] = {0x202d34, 0x75bed3, "TAIL SHAPE"},
    [SPRING_REVERB] = {0x343332, 0xc8ba94, "SPRING TANK"},
    [FILTER] = {0x222a31, 0x71b4c0, "FILTER RESPONSE"},
    [BUS_COMP] = {0x252e33, 0xa3b9ba, "COMPRESSION CURVE"},
    [DUCKER_INPUT] = {0x302a21, 0xd58d39, "SIDECHAIN SEND"},
    [LIMITER] = {0x23272d, 0xa9b2bb, "LIMITER"},
    [PUMPER] = {0x292936, 0xad94ce, "RECOVERY SHAPE"},
    [TRANSIENT] = {0x2c2828, 0xd9a277, "ATTACK / SUSTAIN"},
    [GATE] = {0x253029, 0x85bf8c, "GATE CURVE"},
    [AMP_SIM] = {0x2b2521, 0xe3c68f, "AMP HEAD"},
    [ENSEMBLE] = {0x242d37, 0x79bbc9, "ENSEMBLE VOICES"},
    [CHORUS] = {0x262a36, 0xa4aee7, "CHORUS VOICES"},
    [PHASER] = {0x303335, 0x8ebec9, "PHASE NOTCHES"},
    [FLANGER] = {0x2b3237, 0x7db2dc, "COMB RESPONSE"},
    [WAH] = {0x2b3026, 0xaac77b, "WAH SWEEP"},
    [AUTO_PAN] = {0x272e35, 0x8cbacc, "STEREO PAN"},
    [VINTAGE] = {0x302a34, 0xbb9cc8, "SAMPLER CHARACTER"},
    [VINYL] = {0x302c24, 0xd6b477, "VINYL CHARACTER"},
    [TAPE] = {0x322b23, 0xd59a55, "TAPE CHARACTER"},
    [AIR_FLAVOR] = {0x263238, 0x79beb9, "COLOR PROCESSOR"},
    [HS_CHANNEL] = {0x30363b, 0x929da2, "CHANNEL STRIP"},
    [HS_XSAT] = {0x302b27, 0xd3a164, "SATURATION"},
    [HS_BUS] = {0x283039, 0x9baac0, "BUS COMPRESSOR"},
    [HS_EQ2] = {0x252e36, 0x77bcb0, "PARA EQ"}
};

static void fx_line(uint32_t *p, int x, int y, int ex, int ey, uint32_t color)
{
    int dx = abs(ex-x), sx = x<ex ? 1 : -1;
    int dy = -abs(ey-y), sy = y<ey ? 1 : -1;
    int error = dx+dy;
    for (;;) {
        rect(p,x,y,1,1,color);
        if (x==ex && y==ey) break;
        int twice = error*2;
        if (twice>=dy) { error+=dy; x+=sx; }
        if (twice<=dx) { error+=dx; y+=sy; }
    }
}

static void fx_disc(uint32_t *p, int x, int y, int r, uint32_t color)
{
    for (int row=-r; row<=r; ++row)
        for (int col=-r; col<=r; ++col)
            if (row*row+col*col<=r*r)
                rect(p,x+col,y+row,1,1,color);
}

static void fx_reel(uint32_t *p, int x, int y, uint32_t accent)
{
    fx_disc(p,x,y,13,0x605b50);
    fx_disc(p,x,y,12,0x181c1b);
    fx_disc(p,x,y,10,accent);
    fx_disc(p,x,y,9,0x2b2b26);
    fx_disc(p,x-5,y-3,2,0x0b0f10);
    fx_disc(p,x+5,y-3,2,0x0b0f10);
    fx_disc(p,x,y+6,2,0x0b0f10);
    fx_disc(p,x,y,2,0xb1a89a);
}

static void fx_repeats(uint32_t *p, enum effect kind, const int *values, uint32_t accent)
{
    int tape = kind == TAPE_DELAY;
    int dual = kind == DIFF_DELAY || kind == SAMPLE_DELAY;
    int start = tape ? 119 : 20;
    if (tape) {
        fx_reel(p,35,54,accent);
        fx_reel(p,81,54,accent);
        fx_line(p,35,67,81,67,accent);
        fx_disc(p,58,66,2,0x948779);
    }
    int spacing = values[0] < 0 ? 21 : 15 + values[0]/5;
    int strength = values[1] < 0 ? 50 : 20 + values[1];
    for (int row=0; row<1+dual; ++row) {
        int y = dual ? 54+row*14 : 67;
        rect(p,start,y,300-start,1,0x485457);
        if (kind == SAMPLE_DELAY) {
            int offset = values[row] < 0 ? 0 : values[row];
            rect(p, start+5+offset*3, y-8, 2, 8, accent);
        } else {
            for (int x=start+5+row*7, i=0; x<299 && i<8; x+=spacing, ++i) {
                int height = (dual ? 8 : 17) - i*(95-strength)/24;
                if (height>1) rect(p,x,y-height,2,height,accent);
            }
        }
    }
}

static void fx_plot(uint32_t *p, enum effect kind, const int *values, uint32_t accent)
{
    int prev = 0;
    for (int x=18; x<=302; x+=24) rect(p,x,46,1,24,0x263337);
    for (int y=46; y<=70; y+=8) rect(p,18,y,285,1,0x263337);
    for (int i=0; i<=140; ++i) {
        float t = i/140.0f;
        float value;
        switch (kind) {
        case REVERB: {
            float start = values[0]<0 ? 0 : values[0]/500.0f;
            float decay = values[1]<0 ? 4.0f : 7.0f-values[1]/15.0f;
            value = t < start ? 0 : expf(-(t-start)*decay);
            break;
        }
        case BUS_COMP: value = t < .5f ? t : .5f+(t-.5f)*.38f; break;
        case PUMPER: value = 1.0f-expf(-4.0f*t); break;
        case TRANSIENT: value = t<.13f ? t/.13f : .25f+.75f*expf(-7*(t-.13f)); break;
        case GATE: value = t<.4f ? 0.0f : .35f+(t-.4f)*.9f; break;
        case PHASER: value = .85f-.65f*powf(sinf(t*9.4248f),8); break;
        case FLANGER: value = .85f-.65f*powf(sinf(t*25.1327f),6); break;
        case WAH: value = .10f+.8f*expf(-65*(t-.5f)*(t-.5f)); break;
        default: value = .5f+.25f*sinf(t*12.5664f); break;
        }
        int y=69-(int)lroundf(20*value);
        int x=19+i*2;
        if (i) fx_line(p,x-2,prev,x,y,accent);
        prev=y;
    }
    if (kind == ENSEMBLE || kind == CHORUS)
        for (int x=19; x<=299; ++x)
            rect(p,x,59+(int)lroundf(7*cosf(x*.044f)),1,1,0x658594);
}

static const uint32_t channel_colors[] = {
    0xa1adb1, 0xe0a353, 0xa58d63, 0x69a9d1, 0x70b982, 0xd66c61, 0xddbd68, 0x7cb6a7
};

static int channel_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_channel_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "INPUT / OUTPUT";
    return section >= 0 && section < 8 ? section : 0;
}

static void channel_panel(uint32_t *p)
{
    static const char *const tabs[] = {"I/O", "FILT", "LF", "LM", "HM", "HF", "COMP", "GATE"};
    const char *name;
    int section = channel_section(&name);
    uint32_t accent = channel_colors[section];
    text(p, 15, 39, name, 19, 0xd9e0de);
    text(p, 263, 39, "2 PAGE", 6, 0xb8c7ca);
    if (section >= 6) text(p, 207, 39, "SHIFT", 5, 0xc3b793);
    for (int i = 0; i < 8; ++i) {
        int x = 12 + i * 37;
        int selected = i == section;
        rect(p, x, 52, 35, 20, selected ? channel_colors[i] : 0x242b31);
        frame(p, x, 52, 35, 20, selected ? 0xdfe6e5 : 0x505b62);
        rect(p, x + 1, 53, 33, 2, channel_colors[i]);
        text(p, x + (35 - 4*(int)strlen(tabs[i])) / 2, 60, tabs[i], 5,
             selected ? 0x182028 : 0xd7dfe0);
    }
    rect(p, 12, 48, 294, 1, accent);
}

static void rack_knob(uint32_t *p, int x, int y, int value, int bypass, uint32_t accent)
{
    if (bypass) accent = 0x6e787d;
    fx_disc(p, x, y+1, 18, 0x090e10);
    fx_disc(p, x, y, 16, 0x778287);
    fx_disc(p, x, y+1, 15, 0x22282c);
    fx_disc(p, x, y, 12, 0x151b20);
    fx_disc(p, x, y, 9, accent);
    fx_disc(p, x, y+1, 7, accent);
    for (int i = 0; i < 9; ++i) {
        float angle = -2.3561945f + i * 0.5890486f;
        rect(p, x + (int)lroundf(sinf(angle)*20), y - (int)lroundf(cosf(angle)*20),
             1, 1, i == 4 ? 0xced2cd : 0x778187);
    }
    if (value >= 0) {
        float angle = -2.3561945f + value * (4.712389f / 73);
        float dx = sinf(angle), dy = cosf(angle);
        fx_line(p, x+(int)lroundf(dx*4), y-(int)lroundf(dy*4),
                x+(int)lroundf(dx*12), y-(int)lroundf(dy*12), 0xf1efe4);
    }
}

static void channel_knob(uint32_t *p, int x, int y, int value, int bypass)
{
    rack_knob(p, x, y, value, bypass, channel_colors[channel_section(NULL)]);
}

static void xsat_panel(uint32_t *p)
{
    static const char *const tabs[] = {"DRIVE", "HARMONICS", "I/O"};
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_xsat_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    text(p, 15, 39, title ? title : "DRIVE", 19, 0xe2ddca);
    text(p, 263, 39, "2 PAGE", 6, 0xb8c7ca);
    rect(p, 12, 48, 294, 1, 0xd3a164);
    for (int i = 0; i < 3; ++i) {
        int x = 12 + i * 99;
        int active = i == section;
        rect(p, x, 52, 96, 20, active ? 0xd3a164 : 0x242b31);
        frame(p, x, 52, 96, 20, active ? 0xffd894 : 0x6b6053);
        text(p, x + (96 - 4*(int)strlen(tabs[i])) / 2, 60, tabs[i], 9,
             active ? 0x201b17 : 0xe3d9c7);
    }
}

static int bus_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_bus_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "COMPRESSION";
    return section >= 0 && section < 3 ? section : 0;
}

static const char *bus_marking(int knob, int value)
{
    static const char *const ratio[] = {"1.5:1","2:1","3:1","4:1","10:1","20:1","X"};
    static const char *const attack[] = {"0.1 MS","0.3 MS","1 MS","3 MS","10 MS","20 MS","30 MS"};
    static const char *const release[] = {"0.1 S","0.3 S","0.4 S","0.6 S","0.8 S","1.2 S","AUTO"};
    if (value < 0) return NULL;
    int step = (value*6+36)/73;
    int section = bus_section(NULL);
    if (section == 0 && knob == 1) return ratio[step];
    if (section == 1 && knob == 0) return attack[step];
    if (section == 1 && knob == 1) return release[step];
    if (section == 1 && knob == 2 && value <= 3) return "OFF";
    if (section == 1 && knob == 2 && value <= 7) return "20 HZ";
    if (section == 2 && knob == 1) return value >= 37 ? "ON" : "OFF";
    return NULL;
}

static void bus_panel(uint32_t *p)
{
    static const char *const tabs[] = {"COMPRESS", "TIMING", "PARALLEL"};
    const char *name;
    int section = bus_section(&name);
    text(p, 15, 39, name, 19, 0xd9e0de);
    text(p, 263, 39, "2 PAGE", 6, 0xb8c7ca);
    rect(p, 12, 48, 294, 1, 0x9baac0);
    for (int i = 0; i < 3; ++i) {
        int x = 12 + i*99, active = i == section;
        rect(p, x, 52, 96, 20, active ? 0x9baac0 : 0x202931);
        frame(p, x, 52, 96, 20, active ? 0xc4d0dc : 0x596672);
        text(p, x+(96-4*(int)strlen(tabs[i]))/2, 60, tabs[i], 9,
             active ? 0x182028 : 0xd7dfe0);
    }
}

static int eq2_position(int *page)
{
    typedef int (*read_fn)(int *);
    static read_fn read_state;
    if (!read_state) read_state=(read_fn)dlsym(RTLD_DEFAULT,"hs_eq2_selection");
    *page=0;
    int band=read_state ? read_state(page) : 1;
    return band>=0 && band<=EQ_BANDS ? band : 1;
}

static uint32_t eq2_color(void)
{
    static const uint32_t colors[]={0x79bcab,0x7eaed0,0xd5aa73,0xbfa6d2};
    int page;
    eq2_position(&page);
    return colors[page&3];
}

static const char *eq2_marking(int knob,int value)
{
    static const char *const type[]={"HIGH PASS","LOW PASS","LOW SHELF","HIGH SHELF","BELL"};
    static const char *const family[]={"CUSTOM Q","CRITICAL","BESSEL","GAUSSIAN","BUTTERWORTH","CHEBYSHEV"};
    static const char *const roll[]={"OFF","6 DB/OCT","12 DB/OCT","18 DB/OCT","24 DB/OCT",
                                    "30 DB/OCT","36 DB/OCT","42 DB/OCT","48 DB/OCT"};
    static const char *const bell[]={"CLASSIC SYM","CLASSIC ASYM","PROP 1","PROP 2","PROP 3",
                                    "CONST Q ASYM","Q ASYM R","Q INVERT","CONST Q NEW"};
    static const char *const stereo[]={"LEFT/MID","RIGHT/SIDE","STEREO"};
    if (value<0) return NULL;
    int page,band=eq2_position(&page);
    const char *sw=value>=37 ? "ON" : "OFF";
    if (!band) return (page==0 ? knob==2 : knob<=1) ? sw : NULL;
    if (page==1) return knob==0 ? type[(value*4+36)/73] : sw;
    if (page==2) {
        if (knob==0) return family[(value*5+36)/73];
        return knob==1 ? roll[(value*8+36)/73] : bell[(value*8+36)/73];
    }
    if (page==3 && knob==0) return stereo[(value*2+36)/73];
    return NULL;
}

static void eq2_panel(uint32_t *p)
{
    static const char *const band_tabs[]={"SHAPE","MODE","FILTER","STEREO"};
    static const char *const global_tabs[]={"LEVELS","ROUTING"};
    int page,band=eq2_position(&page);
    uint32_t color=eq2_color();
    char title[]="BAND 01";
    title[5]='0'+band/10; title[6]='0'+band%10;
    text(p,15,39,band ? title : "GLOBAL",12,0xe1ece7);
    text(p,203,39,"ENC BAND  2 PAGE",17,0xb8c7ca);
    for (int i=0;i<=EQ_BANDS;++i) {
        int x=12+12*i;
        rect(p,x,49,10,7,i==band ? color : i%4==0 ? 0x52616b : 0x303d44);
    }
    int tabs=band ? 4 : 2;
    for (int i=0;i<tabs;++i) {
        int w=296/tabs, x=12+i*w, active=i==page;
        const char *label=band ? band_tabs[i] : global_tabs[i];
        rect(p,x,60,w-3,13,active ? color : 0x242c33);
        frame(p,x,60,w-3,13,active ? 0xc4ded8 : 0x53616a);
        text(p,x+(w-3-text_width(label,12))/2,64,label,12,active ? 0x162620 : 0xdbe8e8);
    }
}

static void fx_faceplate_art(uint32_t *p, const struct effect_page *page)
{
    enum effect kind = page->kind;
    const struct faceplate *face = &faceplates[kind];
    uint32_t accent = face->accent;
    rect(p,8,34,304,42,0x101719);
    frame(p,8,34,304,42,0x465259);
    if (kind != TAPE_DELAY && kind != TAPE && kind != AIR_FLAVOR && kind != HS_CHANNEL && kind != HS_XSAT && kind != HS_BUS && kind != HS_EQ2)
        text(p,17,38,face->legend,24,0xaebfc2);

    switch (kind) {
    case TAPE_DELAY:
        text(p,119,38,"TAPE ECHO / REPEATS",24,0xbcb4a4);
        fx_repeats(p,kind,page->main_values,accent);
        break;
    case DELAY: case DIFF_DELAY: case SAMPLE_DELAY:
        fx_repeats(p,kind,page->main_values,accent);
        break;
    case SPRING_REVERB:
        for (int x=20; x<297; ++x) {
            int y=59+(int)lroundf(6*sinf(x*.6f));
            rect(p,x,y,1,2,accent);
        }
        rect(p,17,51,4,16,0x79786c);
        rect(p,297,51,4,16,0x79786c);
        break;
    case AMP_SIM:
        for (int y=47; y<68; y+=3)
            for (int x=19+(y%2); x<301; x+=4)
                rect(p,x,y,2,1,0x514738);
        rect(p,17,69,285,1,accent);
        break;
    case AUTO_PAN:
        text(p,17,58,"L",1,accent);
        text(p,298,58,"R",1,accent);
        for (int x=28; x<293; ++x)
            rect(p,x,66-(int)lroundf(17*sinf((x-28)*3.14159f/264)),1,1,accent);
        rect(p,159,47,3,10,accent);
        break;
    case VINTAGE:
        for (int i=0; i<14; ++i) {
            int y=54+(i%4)*4;
            fx_line(p,20+i*20,y,39+i*20,y,accent);
            if (i<13) fx_line(p,39+i*20,y,39+i*20,54+((i+1)%4)*4,accent);
        }
        break;
    case VINYL:
        for (int r=14; r>=2; r-=2) {
            fx_disc(p,280,55,r,0x6f6346);
            fx_disc(p,280,55,r-1,0x192022);
        }
        fx_disc(p,280,55,4,accent);
        for (int x=19; x<250; x+=4) rect(p,x,64,2,1,0x615a4a);
        break;
    case HS_EQ2:
        eq2_panel(p);
        break;
    case HS_BUS:
        bus_panel(p);
        break;
    case HS_XSAT:
        xsat_panel(p);
        break;
    case HS_CHANNEL:
        channel_panel(p);
        break;
    case AIR_FLAVOR: {
        static const char *const modules[] = {"PITCH", "DISTORT", "DIGITAL", "VINYL", "VOLUME", "TIMBRE"};
        typedef const char *(*bank_fn)(int *);
        static bank_fn current_bank;
        if (!current_bank) current_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_flavor_bank");
        int selected = -1;
        const char *title = current_bank ? current_bank(&selected) : NULL;
        flavor_sprite(p, 9, 35, 302, 13, flavor_header);
        text(p, 15, 38, title ? title : "GLOBAL", 19, 0xe5efe8);
        text(p, 241, 38, "2 PAGE", 7, 0xd4eeeb);
        flavor_sprite(p, 12, 49, 296, 23, flavor_rail);
        for (int i = 0; i < 6; ++i) {
            int x = 12 + 49 * i;
            rect(p, x+1, 65, 47, 7, i == selected ? 0xe5e2ce : 0x353a3e);
            text(p, x+4, 66, modules[i], 7, i == selected ? 0x303737 : 0xe7e5db);
            if (i == selected) frame(p, x, 48, 49, 26, 0xf6edce);
        }
        break;
    }
    case TAPE:
        fx_reel(p,35,54,accent);
        fx_reel(p,81,54,accent);
        fx_line(p,35,67,81,67,accent);
        rect(p,53,62,10,9,0x777264);
        text(p,119,47,"TAPE CHARACTER",20,0xbcb4a4);
        rect(p,119,61,177,1,accent);
        break;
    default:
        fx_plot(p,kind,page->main_values,accent);
        break;
    }
}

#endif
