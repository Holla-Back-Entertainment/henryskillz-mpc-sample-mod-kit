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
    [HS_EQ2] = {0x252e36, 0x77bcb0, "PARA EQ"},
    [HS_PUMP] = {0x060708, 0x2fa8e8, "PUMP"},
    [HS_HEAT] = {0x0c0c0d, 0xe8e6e0, "HEAT"},
    [HS_DISTRESSOR] = {0x0a0a0b, 0xd8d8d4, "CRUSH"},
    [HS_BASS_FOCUS] = {0x0b0e0f, 0x48c8a0, "BASS FOCUS"},
    [HS_TUBE_STRIP] = {0x0d1116, 0xf0a040, "TUBE STRIP"}
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
    rect(p, 12, 48, 294, 1, 0x9baac0);
    for (int i = 0; i < 3; ++i) {
        int x = 12 + i*99, active = i == section;
        rect(p, x, 52, 96, 20, active ? 0x9baac0 : 0x202931);
        frame(p, x, 52, 96, 20, active ? 0xc4d0dc : 0x596672);
        text(p, x+(96-4*(int)strlen(tabs[i]))/2, 60, tabs[i], 9,
             active ? 0x182028 : 0xd7dfe0);
    }
}

static int pump_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_pump_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "COMPRESS";
    return section >= 0 && section < 3 ? section : 0;
}

/* Panel styled after the hardware's look: black face, blue print, black knobs with white
   pointers, the ratio LED column (laid flat) and the Det/Sat lamps. Ratio, Det and Sat are
   only visible on their own page, so the last values seen are remembered for the lamps. */
static const uint32_t pump_blue = 0x2fa8e8;
static int pump_ratio = 3, pump_det, pump_sat;

static void pump_note(int knob, int value)
{
    int section = pump_section(NULL);
    if (value < 0) return;
    if (section == 0 && knob == 1) pump_ratio = (value*8+36)/73;
    if (section == 2 && knob == 0) pump_sat = value >= 37;
    if (section == 2 && knob == 1) pump_det = value >= 37;
}

static const char *pump_marking(int knob, int value)
{
    static const char *const ratio[] = {"1:1","2:1","3:1","4:1","6:1","8:1","10:1","12:1","20:1"};
    static const char *const attack[] = {"0.2 MS","1 MS","3 MS","6 MS","10 MS","20 MS","30 MS","40 MS"};
    static const char *const release[] = {"50 MS","0.1 S","0.15 S","0.2 S","0.25 S","0.3 S","0.5 S","OPTO"};
    if (value < 0) return NULL;
    int section = pump_section(NULL);
    if (section == 0 && knob == 1) return ratio[(value*8+36)/73];
    if (section == 1 && knob == 0) return attack[(value*7+36)/73];
    if (section == 1 && knob == 1) return release[(value*7+36)/73];
    if (section == 1 && knob == 2 && value == 0) return "OFF";
    if (section == 2 && knob < 2) return value >= 37 ? "ON" : "OFF";
    return NULL;
}

static void pump_led(uint32_t *p, int x, int y, uint32_t color, int lit)
{
    uint32_t dim = ((color >> 2) & 0x3f3f3f);
    fx_disc(p, x, y, 4, 0x050506);
    fx_disc(p, x, y, 3, lit ? color : dim);
    if (lit) rect(p, x-1, y-2, 2, 1, 0xffffff);
}

static void pump_knob(uint32_t *p, int x, int y, int value, int bypass)
{
    fx_disc(p, x, y+1, 18, 0x000000);
    fx_disc(p, x, y, 16, 0x2b2d31);
    fx_disc(p, x, y, 14, 0x141518);
    for (int i = 0; i < 24; ++i) {   /* knurled edge */
        float a = i * .2617994f;
        rect(p, x + (int)lroundf(sinf(a)*15), y - (int)lroundf(cosf(a)*15), 1, 1, 0x3a3d42);
    }
    fx_disc(p, x, y, 10, 0x1c1d20);
    for (int i = 0; i < 11; ++i) {   /* white scale ticks, 0..10 */
        float a = -2.3561945f + i * .4712389f;
        rect(p, x + (int)lroundf(sinf(a)*20), y - (int)lroundf(cosf(a)*20), 1, 1, 0xe8e8e8);
    }
    if (value >= 0) {
        float a = -2.3561945f + value * (4.712389f / 73);
        float dx = sinf(a), dy = cosf(a);
        uint32_t c = bypass ? 0x7a7f84 : 0xf4f4f0;
        fx_line(p, x+(int)lroundf(dx*3), y-(int)lroundf(dy*3), x+(int)lroundf(dx*14), y-(int)lroundf(dy*14), c);
        fx_line(p, x+(int)lroundf(dx*3)+1, y-(int)lroundf(dy*3), x+(int)lroundf(dx*14)+1, y-(int)lroundf(dy*14), c);
    }
}

static void pump_panel(uint32_t *p, const int *values)
{
    for (int i = 0; i < 3; ++i) pump_note(i, values[i]);
    static const char *const tabs[] = {"COMP", "TIME", "COLOUR"};
    static const char *const ratios[] = {"1","2","3","4","6","8","10","12","20"};
    static const uint32_t colors[] = {0x46c84a,0x46c84a,0x46c84a,0x46c84a,0xe2d23a,0xe2d23a,0xf08a2c,0xf08a2c,0xe8382c};
    const char *name;
    int section = pump_section(&name);
    rect(p, 8, 34, 304, 42, 0x08090b);
    frame(p, 8, 34, 304, 42, pump_blue);
    text(p, 15, 39, "HS PUMP", 7, pump_blue);
    rect(p, 15, 46, 27, 1, pump_blue);
    text(p, 51, 39, name, 19, 0xf0f0ec);
    /* ratio LEDs, as on the hardware's left column, laid flat */
    text(p, 15, 52, "RATIO", 5, 0xf0f0ec);
    for (int i = 0; i < 9; ++i) {
        int x = 46 + i*17;
        pump_led(p, x, 55, colors[i], i == pump_ratio);
        text(p, x - 2*(int)strlen(ratios[i]) + 1, 63, ratios[i], 2, 0xd0d0cc);
    }
    rect(p, 15, 70, 180, 1, pump_blue);
    /* Det / Sat lamps */
    text(p, 206, 52, "DET", 3, 0xf0f0ec);
    pump_led(p, 225, 55, 0x46c84a, pump_det);
    text(p, 236, 52, "SAT", 3, 0xf0f0ec);
    pump_led(p, 255, 55, 0xe8382c, pump_sat);
    /* page lamps (B2 flips pages) */
    for (int i = 0; i < 3; ++i) {
        int x = 206 + i*35, active = i == section;
        rect(p, x, 63, 32, 9, active ? pump_blue : 0x101317);
        frame(p, x, 63, 32, 9, active ? 0xbfe6fa : 0x2a5f7e);
        text(p, x + (32 - 4*(int)strlen(tabs[i]))/2 + 1, 65, tabs[i], 6, active ? 0x050608 : 0xb8d8ea);
    }
}

/* HS Heat: black face, light grey knobs with white dots, amber lamps for the 8 circuits and
   the 7 filter types, and a yellow-green screen for the page name, as on the hardware it is
   styled after. Circuit and filter type are remembered for the lamps when off their page. */
static const uint32_t heat_lcd = 0xd8f040, heat_amber = 0xf2a23a;
static int heat_circuit = 1, heat_filter;

static int heat_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_heat_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "CHARACTER";
    return section >= 0 && section < 6 ? section : 0;
}

static void heat_note(int knob, int value)
{
    if (value < 0) return;
    int section = heat_section(NULL);
    if (section == 0 && knob == 0) heat_circuit = (value*7+36)/73;
    if (section == 1 && knob == 0) heat_filter = (value*7+36)/73;
}

static const char *heat_marking(int knob, int value)
{
    static const char *const circuit[] = {"CLEAN BOOST","SATURATION","ENHANCEMENT","MID DRIVE",
                                          "ROUGH CRUNCH","CLASSIC DIST","ROUND FUZZ","HIGH GAIN"};
    static const char *const filter[] = {"OFF","LP2 12 DB","LP1 6 DB","BAND PASS","HP1 6 DB",
                                         "HP2 12 DB","NOTCH","PEAK"};
    static const char *const wave[] = {"TRI","SINE","SQR","SAW","EXP","RAMP","RND"};
    if (value < 0) return NULL;
    int section = heat_section(NULL);
    if (section == 0 && knob == 0) return circuit[(value*7+36)/73];
    if (section == 1 && knob == 0) return filter[(value*7+36)/73];
    if (section == 4 && knob == 2) return wave[(value*6+36)/73];
    return NULL;
}

static void heat_knob(uint32_t *p, int x, int y, int value, int bypass)
{
    for (int i = 0; i < 11; ++i) {   /* white tick marks */
        float a = -2.3561945f + i * .4712389f;
        fx_line(p, x + (int)lroundf(sinf(a)*19), y - (int)lroundf(cosf(a)*19),
                x + (int)lroundf(sinf(a)*22), y - (int)lroundf(cosf(a)*22), 0xd8d8d4);
    }
    fx_disc(p, x, y+2, 16, 0x050505);       /* drop shadow */
    fx_disc(p, x, y, 15, 0x9d9d99);
    fx_disc(p, x, y-1, 14, 0xcfcfcb);
    fx_disc(p, x, y-1, 11, 0xd9d9d5);
    if (value >= 0) {
        float a = -2.3561945f + value * (4.712389f / 73);
        int dx = (int)lroundf(sinf(a)*10), dy = (int)lroundf(cosf(a)*10);
        fx_disc(p, x+dx, y-1-dy, 3, bypass ? 0x8a8a86 : 0x2a2a2a);
        fx_disc(p, x+dx, y-1-dy, 2, bypass ? 0xb0b0ac : 0xffffff);
    }
}

static void heat_lamp(uint32_t *p, int x, int y, int lit)
{
    fx_disc(p, x, y, 3, 0x2a1408);
    fx_disc(p, x, y, 2, lit ? heat_amber : 0x6a3416);
    if (lit) rect(p, x-1, y-1, 1, 1, 0xfff0b0);
}

/* the seven filter-type pictograms from the hardware, 9x5 each */
static void heat_icon(uint32_t *p, int x, int y, int type, uint32_t c)
{
    switch (type) {
    case 0: fx_line(p,x,y,x+5,y,c); fx_line(p,x+5,y,x+8,y+4,c); break;              /* LP2 */
    case 1: fx_line(p,x,y,x+3,y,c); fx_line(p,x+3,y,x+8,y+4,c); break;              /* LP1 */
    case 2: fx_line(p,x,y+4,x+4,y,c); fx_line(p,x+4,y,x+8,y+4,c); break;            /* BP */
    case 3: fx_line(p,x,y+4,x+5,y,c); fx_line(p,x+5,y,x+8,y,c); break;              /* HP1 */
    case 4: fx_line(p,x,y+4,x+3,y,c); fx_line(p,x+3,y,x+8,y,c); break;              /* HP2 */
    case 5: fx_line(p,x,y,x+3,y,c); fx_line(p,x+3,y,x+4,y+4,c); fx_line(p,x+4,y+4,x+5,y,c); fx_line(p,x+5,y,x+8,y,c); break;
    default: fx_line(p,x,y+3,x+3,y+3,c); fx_line(p,x+3,y+3,x+4,y,c); fx_line(p,x+4,y,x+5,y+3,c); fx_line(p,x+5,y+3,x+8,y+3,c);
    }
}

static void heat_panel(uint32_t *p, const int *values)
{
    static const char *const short_names[] = {"CLN","SAT","ENH","MID","CRN","DST","FUZ","HG"};
    for (int i = 0; i < 3; ++i) heat_note(i, values[i]);
    const char *name;
    int section = heat_section(&name);
    rect(p, 8, 34, 304, 42, 0x0c0c0d);
    frame(p, 8, 34, 304, 42, 0x3c3c3e);
    text(p, 14, 38, "HS HEAT", 7, 0xf2f2ee);
    /* yellow-green screen with the page name and position */
    rect(p, 100, 36, 206, 11, 0x0b0d02);
    frame(p, 100, 36, 206, 11, 0x5d6a14);
    char pos[8] = {(char)('1'+section), '/', '6', 0};
    text(p, 104, 39, name, 19, heat_lcd);
    text(p, 286, 39, pos, 3, heat_lcd);
    /* CHARACTER: one lamp per circuit */
    rect(p, 14, 50, 160, 1, 0x5a5a5c);
    for (int i = 0; i < 8; ++i) {
        int x = 22 + i*20;
        heat_lamp(p, x, 57, i == heat_circuit);
        text(p, x - 2*(int)strlen(short_names[i]) + 1, 64, short_names[i], 3, 0xd6d6d2);
    }
    /* FILTER: pictogram + lamp per type; all dark when the filter is off */
    rect(p, 184, 50, 122, 1, 0x5a5a5c);
    for (int i = 0; i < 7; ++i) {
        int x = 192 + i*17;
        heat_icon(p, x - 4, 54, i, 0xd6d6d2);
        heat_lamp(p, x, 66, heat_filter == i + 1);
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
    text(p,239,39,"ENC BAND",8,0xb8c7ca);
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

/* HS Crush: black face, white print, a red gain-reduction LED row, the ratio LEDs laid flat,
   and lamps for the Detector, Audio and British buttons. The GR
   row reads the open instance (dist_gr_db, set in effect_knobs); -1 means it could not be read. */
static float dist_gr_db = -1;
static int dist_ratio = 4, dist_det, dist_audio, dist_brit;

static int dist_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_distressor_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "COMPRESS";
    return section >= 0 && section < 3 ? section : 0;
}

static void dist_note(int knob, int value)
{
    int section = dist_section(NULL);
    if (value < 0) return;
    if (section == 0 && knob == 1) dist_ratio = (value*7+36)/73;
    if (section == 1 && knob == 2) dist_det = (value*3+36)/73;
    if (section == 2 && knob == 0) dist_audio = (value*5+36)/73;
    if (section == 2 && knob == 1) dist_brit = value >= 37;
}

static const char *dist_marking(int knob, int value)
{
    static const char *const ratio[] = {"1:1","2:1","3:1","4:1","6:1","10:1 OPTO","20:1","MAX"};
    static const char *const det[] = {"NORM","HP","BAND","HP + BAND"};
    static const char *const audio[] = {"CLEAN","HP","DIST 2","DIST 3","HP + DIST 2","HP + DIST 3"};
    static char s[3][16];
    if (value < 0) return NULL;
    int section = dist_section(NULL);
    float k = value*10.0f/73;
    if (section == 0 && knob == 1) return ratio[(value*7+36)/73];
    if (section == 1 && knob == 2) return det[(value*3+36)/73];
    if (section == 2 && knob == 0) return audio[(value*5+36)/73];
    if (section == 2 && knob == 1) return value >= 37 ? "ON" : "OFF";
    if (section == 2) return NULL;
    if (section == 1 && knob == 0) {
        float ms = .05f*powf(600, k*.1f);
        snprintf(s[0], sizeof s[0], ms < 1 ? "%.1f  %.0f US" : "%.1f  %.1f MS", k, ms < 1 ? ms*1000 : ms);
    } else if (section == 1 && knob == 1) {
        float sec = .05f*powf(dist_ratio == 5 ? 400 : 70, k*.1f);
        snprintf(s[1], sizeof s[1], "%.1f  %.2f S", k, sec);
    } else {
        snprintf(s[2], sizeof s[2], "%.1f", k);
        return s[2];
    }
    return s[knob];
}

static void dist_panel(uint32_t *p, const int *values)
{
    for (int i = 0; i < 3; ++i) dist_note(i, values[i]);
    static const char *const ratios[] = {"1","2","3","4","6","OPT","20","MAX"};
    static const int gr_steps[] = {1, 2, 3, 4, 6, 8, 10, 14, 20, 30};
    const uint32_t ink = 0xeeeeea, dim = 0x8a8a86;
    const char *name;
    dist_section(&name);
    rect(p, 8, 34, 304, 42, 0x0a0a0b);
    frame(p, 8, 34, 304, 42, 0x4a4a4c);
    text(p, 15, 38, "HS CRUSH", 8, ink);
    rect(p, 15, 45, 31, 1, 0xe8382c);
    text(p, 56, 38, name, 19, ink);
    /* gain reduction, red LEDs */
    text(p, 15, 50, "GR DB", 5, dim);
    for (int i = 0; i < 10; ++i) {
        int x = 19 + i*9;
        pump_led(p, x, 59, 0xe8382c, dist_gr_db >= gr_steps[i]);
        if (i == 0 || i == 4 || i == 9) {
            char n[3]; snprintf(n, sizeof n, "%d", gr_steps[i]);
            text(p, x - 2*(int)strlen(n) + 1, 64, n, 2, dim);
        }
    }
    if (dist_gr_db < 0) text(p, 45, 50, "NO METER", 8, 0x6a6a68);
    /* ratio LEDs; British mode lights its own lamp instead */
    text(p, 116, 50, "RATIO", 5, dim);
    for (int i = 0; i < 8; ++i) {
        int x = 121 + i*17;
        uint32_t c = i < 4 ? 0x46c84a : i < 6 ? 0xe2d23a : 0xe8382c;
        pump_led(p, x, 59, c, !dist_brit && i == dist_ratio);
        text(p, x - 2*(int)strlen(ratios[i]) + 1, 64, ratios[i], 3, dim);
    }
    rect(p, 113, 70, 140, 1, 0x2a2a2c);
    /* buttons: detector and audio lamps, British */
    text(p, 15, 70, "DET", 3, dim);
    pump_led(p, 34, 72, 0x46c84a, dist_det & 1);   text(p, 39, 70, "HP", 2, ink);
    pump_led(p, 54, 72, 0xe2d23a, dist_det & 2);   text(p, 59, 70, "BAND", 4, ink);
    int a = dist_audio;
    text(p, 262, 50, "AUDIO", 5, dim);
    pump_led(p, 266, 59, 0x46c84a, a == 1 || a >= 4); text(p, 271, 57, "HP", 2, ink);
    pump_led(p, 266, 66, 0xe2d23a, a == 2 || a == 4); text(p, 271, 64, "D2", 2, ink);
    pump_led(p, 288, 59, 0xe8382c, a == 3 || a == 5); text(p, 293, 57, "D3", 2, ink);
    pump_led(p, 288, 66, 0xf08a2c, dist_brit);       text(p, 293, 64, "BRIT", 4, ink);
}

/* HS Bass Focus: the response it applies, drawn live from the knobs. Log frequency 20 Hz - 20 kHz,
   +8 to -24 dB. The LR4 bands are in phase, so the magnitude is (g + t)/(1 + t) with t = (f/fc)^4. */
static const uint32_t bf_green = 0x48c8a0;
static float bf_freq(int v) { return 30*powf(500/30.0f, v/73.0f); }
static float bf_bass(int v) { return roundf(2*(-24 + 32*v/73.0f))*.5f + 0.0f; }   /* the panel sees 74 steps: show 0.5 dB */
static const char *bf_marking(int knob, int value)
{
    static char s[2][12];
    if (value < 0) return NULL;
    if (knob == 2) return value >= 37 ? "MONO" : "STEREO";
    if (knob == 0) snprintf(s[0], sizeof s[0], "%d HZ", (int)lroundf(bf_freq(value)));
    else { float b = bf_bass(value); snprintf(s[1], sizeof s[1], b ? "%+.1f DB" : "0.0 DB", b); }
    return s[knob];
}
static void bf_panel(uint32_t *p, const int *values)
{
    float fc = bf_freq(values[0] < 0 ? 37 : values[0]), bass = bf_bass(values[1] < 0 ? 55 : values[1]);
    int mono = values[2] < 0 || values[2] >= 37;
    const uint32_t ink = 0xeef2f0, dim = 0x7e8a88;
    char n[16];
    rect(p, 8, 34, 304, 42, 0x0b0e0f);
    frame(p, 8, 34, 304, 42, 0x3c4846);
    text(p, 15, 38, "HS BASS FOCUS", 13, ink);
    rect(p, 15, 45, 51, 1, bf_green);
    text(p, 15, 50, "XOVER", 5, dim);
    snprintf(n, sizeof n, "%d HZ", (int)lroundf(fc));   text(p, 42, 50, n, 7, ink);
    text(p, 15, 58, "BASS", 4, dim);
    snprintf(n, sizeof n, bass ? "%+.1f DB" : "0.0 DB", bass);           text(p, 42, 58, n, 8, ink);
    pump_led(p, 19, 68, bf_green, mono);               text(p, 24, 66, "MONO", 4, mono ? ink : dim);
    pump_led(p, 51, 68, 0xe2d23a, !mono);              text(p, 56, 66, "STEREO", 6, mono ? dim : ink);
    /* response graph */
    enum { GX0 = 98, GX1 = 306, GY0 = 38, GY1 = 66 };
    rect(p, GX0, GY0, GX1 - GX0, GY1 - GY0 + 1, 0x050707);
    float span = log10f(1000);                                    /* 20 Hz .. 20 kHz */
    #define BF_X(f) (GX0 + (int)lroundf((log10f((f)/20) / span) * (GX1 - GX0 - 1)))
    #define BF_Y(db) (GY0 + (int)lroundf((8 - (db)) * (GY1 - GY0) / 32))
    static const int grid[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (unsigned i = 0; i < sizeof grid / sizeof *grid; ++i) rect(p, BF_X(grid[i]), GY0, 1, GY1 - GY0 + 1, 0x141a1a);
    for (int db = 0; db >= -24; db -= 12) rect(p, GX0, BF_Y(db), GX1 - GX0, 1, db ? 0x141a1a : 0x2c3634);
    text(p, BF_X(100) - 5, GY1 + 3, "100", 3, dim);
    text(p, BF_X(1000) - 3, GY1 + 3, "1K", 2, dim);
    text(p, BF_X(10000) - 5, GY1 + 3, "10K", 3, dim);
    float g = powf(10, bass/20);
    int y0 = BF_Y(0), prev = -1;
    for (int x = GX0; x < GX1; ++x) {
        float f = 20*powf(10, span*(x - GX0)/(GX1 - GX0 - 1)), t = powf(f/fc, 4);
        float db = 20*log10f(fmaxf(1e-4f, (g + t)/(1 + t)));
        int y = BF_Y(fmaxf(-24, fminf(8, db)));
        if (f < fc) { int a = y < y0 ? y : y0, b = y < y0 ? y0 : y; rect(p, x, a, 1, b - a + 1, 0x153a30); }
        if (prev >= 0) { int a = prev < y ? prev : y, b = prev < y ? y : prev; rect(p, x, a, 1, b - a + 1, bf_green); }
        else rect(p, x, y, 1, 1, bf_green);
        prev = y;
    }
    int xc = BF_X(fc);
    for (int y = GY0; y <= GY1; y += 2) rect(p, xc, y, 1, 1, ink);
    text(p, GX0 + 3, GY0 + 2, mono ? "M" : "S", 1, mono ? bf_green : 0xe2d23a);
    #undef BF_X
    #undef BF_Y
}

/* HS Tube Strip: blue-black face, amber tube print, a GR LED row read from the open instance
   (tube_gr_db, set in effect_knobs; -1 = could not read), and lamps for HPF, order, SC EQ and Q.
   The lamps remember the last value seen on their page. Knob values arrive as 0-73. */
static const uint32_t tube_amber = 0xf0a040;
static float tube_gr_db = -1;
static int tube_hpf, tube_eq_first, tube_sc, tube_hi_q;

static int tube_section(const char **name)
{
    typedef const char *(*bank_fn)(int *);
    static bank_fn active_bank;
    if (!active_bank) active_bank = (bank_fn)dlsym(RTLD_DEFAULT, "hs_tube_strip_bank");
    int section = 0;
    const char *title = active_bank ? active_bank(&section) : NULL;
    if (name) *name = title ? title : "TUBE";
    return section >= 0 && section < 7 ? section : 0;
}

static float tube_lin(int v, float lo, float hi) { return lo + (hi - lo)*v/73.0f; }
static float tube_log(int v, float lo, float hi) { return lo*powf(hi/lo, v/73.0f); }

static void tube_note(int section, int knob, int value)
{
    if (value < 0) return;
    if (section == 0 && knob == 1) tube_hpf = tube_lin(value, 20, 140) >= 30;
    if (section == 2 && knob == 1) tube_eq_first = value >= 37;
    if (section == 2 && knob == 2) tube_sc = value >= 37;
    if ((section == 4 || section == 5) && knob == 2) tube_hi_q = value >= 37;
}

static const char *tube_hz(char *s, size_t n, float hz)
{
    if (hz >= 1000) snprintf(s, n, hz >= 10000 ? "%.1f KHZ" : "%.2f KHZ", hz/1000);
    else snprintf(s, n, "%d HZ", (int)lroundf(hz));
    return s;
}

static const char *tube_marking(int knob, int value)
{
    static const char *const bass[] = {"15 HZ", "30 HZ", "60 HZ", "150 HZ"};
    static const char *const treble[] = {"10 KHZ", "15 KHZ", "20 KHZ", "32 KHZ"};
    static char s[3][16];
    if (value < 0) return NULL;
    int section = tube_section(NULL);
    char *o = s[knob];
    float x;
    switch (section * 3 + knob) {
    case 0: snprintf(o, 16, "%.1f", tube_lin(value, 0, 10)); break;
    case 1: x = tube_lin(value, 20, 140); if (x < 30) return "OFF"; snprintf(o, 16, "%d HZ", (int)lroundf(x)); break;
    case 2: snprintf(o, 16, "%+.1f DB", tube_lin(value, -20, 20)); break;
    case 3: snprintf(o, 16, "%+.1f DB", tube_lin(value, -30, 20)); break;
    case 4: snprintf(o, 16, "%.1f:1", tube_lin(value, 1, 20)); break;
    case 5: x = tube_log(value, 2, 200); snprintf(o, 16, x < 10 ? "%.1f MS" : "%.0f MS", x); break;
    case 6: x = tube_log(value, 100, 5000); snprintf(o, 16, x < 1000 ? "%.0f MS" : "%.2f S", x < 1000 ? x : x/1000); break;
    case 7: return value >= 37 ? "EQ > COMP" : "COMP > EQ";
    case 8: return value >= 37 ? "ON" : "OFF";
    case 9: return bass[(value*3 + 36)/73];
    case 10: snprintf(o, 16, "%+.1f DB", tube_lin(value, -24, 24)); break;
    case 12: return tube_hz(o, 16, tube_log(value, 30, 4500));
    case 13: case 16: snprintf(o, 16, "%+.1f DB", tube_lin(value, -16, 16)); break;
    case 14: case 17: return value >= 37 ? "HIGH" : "LOW";
    case 15: return tube_hz(o, 16, tube_log(value, 200, 20000));
    case 18: return treble[(value*3 + 36)/73];
    case 19: snprintf(o, 16, "%+.1f DB", tube_lin(value, -20, 20)); break;
    default: return NULL;
    }
    return o;
}

static void tube_panel(uint32_t *p, const int *values)
{
    const char *name;
    int section = tube_section(&name);
    for (int i = 0; i < 3; ++i) tube_note(section, i, values[i]);
    static const int gr_steps[] = {1, 2, 3, 4, 6, 8, 10, 14, 20, 30};
    const uint32_t ink = 0xf2ece0, dim = 0x8a8478;
    rect(p, 8, 34, 304, 42, 0x0d1116);
    frame(p, 8, 34, 304, 42, 0x3a4450);
    text(p, 15, 38, "HS TUBE STRIP", 13, ink);
    rect(p, 15, 45, 51, 1, tube_amber);
    text(p, 78, 38, name, 19, ink);
    text(p, 15, 50, "OPTO GR DB", 10, dim);
    for (int i = 0; i < 10; ++i) {
        int x = 19 + i*9;
        pump_led(p, x, 59, tube_amber, tube_gr_db >= gr_steps[i]);
        if (i == 0 || i == 4 || i == 9) {
            char n[3]; snprintf(n, sizeof n, "%d", gr_steps[i]);
            text(p, x - 2*(int)strlen(n) + 1, 64, n, 2, dim);
        }
    }
    if (tube_gr_db < 0) text(p, 62, 50, "NO METER", 8, 0x6a6a68);
    /* the signal path, left to right, with the order the switch sets */
    static const char *const path[] = {"TUBE", "HPF", "COMP", "EQ"};
    int order[] = {0, 1, tube_eq_first ? 3 : 2, tube_eq_first ? 2 : 3};
    text(p, 116, 50, "SIGNAL PATH", 11, dim);
    for (int i = 0; i < 4; ++i) {
        int x = 116 + i*36, lit = order[i] != 1 || tube_hpf;
        rect(p, x, 58, 30, 9, lit ? 0x2a2014 : 0x15171a);
        frame(p, x, 58, 30, 9, lit ? tube_amber : 0x3a3e44);
        text(p, x + 15 - 2*(int)strlen(path[order[i]]), 60, path[order[i]], 4, lit ? ink : dim);
        if (i < 3) rect(p, x + 30, 62, 6, 1, dim);
    }
    pump_led(p, 266, 59, tube_amber, tube_sc);    text(p, 271, 57, "SC EQ", 5, ink);
    pump_led(p, 266, 66, 0x7fb4e8, tube_hi_q);    text(p, 271, 64, "HI Q", 4, ink);
}

static void fx_faceplate_art(uint32_t *p, const struct effect_page *page)
{
    enum effect kind = page->kind;
    const struct faceplate *face = &faceplates[kind];
    uint32_t accent = face->accent;
    rect(p,8,34,304,42,0x101719);
    frame(p,8,34,304,42,0x465259);
    if (kind != TAPE_DELAY && kind != TAPE && kind != AIR_FLAVOR && kind != HS_CHANNEL && kind != HS_XSAT && kind != HS_BUS && kind != HS_EQ2 && kind != HS_PUMP && kind != HS_HEAT && kind != HS_DISTRESSOR && kind != HS_BASS_FOCUS && kind != HS_TUBE_STRIP)
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
    case HS_PUMP:
        pump_panel(p, page->values);
        break;
    case HS_DISTRESSOR:
        dist_panel(p, page->values);
        break;
    case HS_BASS_FOCUS:
        bf_panel(p, page->values);
        break;
    case HS_TUBE_STRIP:
        tube_panel(p, page->values);
        break;
    case HS_HEAT:
        heat_panel(p, page->values);
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
