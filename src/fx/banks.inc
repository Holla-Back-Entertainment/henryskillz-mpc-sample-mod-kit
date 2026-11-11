struct FlavorBank {
    const char *title;
    int module;
    int parameters[6];
    char labels[6][16];
};

static constexpr FlavorBank banks[] = {
    {"GLOBAL", -1, {6, 10, 7, 8, 9, -1}, {"Intensity", "Width", "Gain", "High-Pass", "Low-Pass", ""}},
    {"PITCH", 0, {0, 11, 12, 13, 16, 14}, {"Power", "Depth", "Vary", "Fast Rate", "Slow Rate", "Drift"}},
    {"PITCH X-MOD", 0, {17, 18, 19, 15, 20, -1}, {"X-Mod", "Type", "Mix", "Stereo", "Fast/Slow", ""}},
    {"DISTORTION", 1, {1, 21, 23, 22, 24, 27}, {"Power", "Depth", "Model", "Vary", "Mix", "Smooth"}},
    {"DIST INPUT", 1, {25, 26, -1, -1, -1, -1}, {"Input HP", "Input LP", "", "", "", ""}},
    {"DIGITAL", 2, {2, 30, 31, 28, 29, 36}, {"Power", "Bits", "Rate", "Depth", "Vary", "Smooth"}},
    {"DIGITAL SHAPE", 2, {32, 33, 37, 34, 35, 38}, {"Glitch", "Length", "Envelope", "Input HP", "Input LP", "Stereo"}},
    {"VINYL", 3, {3, 39, 41, 40, 42, 44}, {"Power", "Depth", "Crackle", "Vary", "Clicks", "Pops"}},
    {"VINYL NOISE", 3, {43, 45, 46, 47, 48, 49}, {"Rate", "Rumble", "Noise", "Tone", "Stereo", "Hum"}},
    {"VINYL ENVELOPE", 3, {50, 51, -1, -1, -1, -1}, {"Envelope", "Release", "", "", "", ""}},
    {"VOLUME", 4, {4, 52, 60, 53, 54, 55}, {"Power", "Depth", "Dropouts", "Vary", "Fast Rate", "Slow Rate"}},
    {"VOLUME SHAPE", 4, {57, 58, 59, 56, -1, -1}, {"Fast Drift", "Slow Drift", "Stereo", "Fast/Slow", "", ""}},
    {"TIMBRE", 5, {5, 61, 63, 62, -1, -1}, {"Power", "Depth", "Profile", "Vary", "", ""}}
};
static constexpr FlavorBank channel_banks[] = {
    {"INPUT / OUTPUT", 0, {CH_INPUT, CH_DRIVE, CH_OUTPUT, -1, -1, -1},
        {"Input dB", "Drive %", "Output dB", "", "", ""}},
    {"242 BLACK FILTERS", 1, {CH_HP, CH_LP, CH_EQ_FIRST, -1, -1, -1},
        {"HP Hz", "LP Hz", "EQ First", "", "", ""}},
    {"LOW EQ", 2, {CH_LF_DB, CH_LF_HZ, CH_LF_BELL, -1, -1, -1},
        {"LF Gain dB", "LF Freq Hz", "LF Bell", "", "", ""}},
    {"LOW MID EQ", 3, {CH_LM_DB, CH_LM_HZ, CH_LM_Q, -1, -1, -1},
        {"LM Gain dB", "LM Freq Hz", "LM Q", "", "", ""}},
    {"HIGH MID EQ", 4, {CH_HM_DB, CH_HM_HZ, CH_HM_Q, -1, -1, -1},
        {"HM Gain dB", "HM Freq Hz", "HM Q", "", "", ""}},
    {"HIGH EQ", 5, {CH_HF_DB, CH_HF_HZ, CH_HF_BELL, -1, -1, -1},
        {"HF Gain dB", "HF Freq Hz", "HF Bell", "", "", ""}},
    {"COMPRESSOR", 6, {CH_COMP_THRESHOLD, CH_COMP_RATIO, CH_MAKEUP,
                        CH_COMP_FAST, CH_COMP_RELEASE, CH_COMP_HARD},
        {"Thresh dB", "Ratio", "Make-Up dB", "Fast Attack", "Release ms", "Hard Knee"}},
    {"GATE / EXPAND", 7, {CH_GATE_THRESHOLD, CH_GATE_RANGE, CH_GATE_RELEASE,
                         CH_GATE_FAST, CH_GATE_EXPAND, -1},
        {"Thresh dB", "Range dB", "Release ms", "Fast Attack", "Expand", ""}}
};
static constexpr FlavorBank xsat_banks[] = {
    {"DRIVE", 0, {XS_DRIVE, XS_SHAPE, XS_DEPTH, -1, -1, -1},
        {"Drive dB", "Shape", "Depth %", "", "", ""}},
    {"HARMONICS", 1, {XS_HARMONICS, XS_MIX, XS_HEADROOM, -1, -1, -1},
        {"Harmonics %", "Mix %", "+6dB Headroom", "", "", ""}},
    {"INPUT / OUTPUT", 2, {XS_INPUT, XS_OUTPUT, XS_BYPASS, -1, -1, -1},
        {"Input dB", "Output dB", "Bypass", "", "", ""}}
};
static constexpr FlavorBank bus_banks[] = {
    {"COMPRESSION", 0, {BUS_THRESHOLD, BUS_RATIO, BUS_MAKEUP, -1, -1, -1},
        {"Threshold dB", "Ratio", "Makeup dB", "", "", ""}},
    {"TIMING", 1, {BUS_ATTACK, BUS_RELEASE, BUS_HPF, -1, -1, -1},
        {"Attack", "Release", "SC HPF Hz", "", "", ""}},
    {"PARALLEL", 2, {BUS_MIX, BUS_BYPASS, -1, -1, -1, -1},
        {"Mix %", "Bypass", "", "", "", ""}}
};
static constexpr FlavorBank pump_banks[] = {
    {"COMPRESS", 0, {PU_INPUT, PU_RATIO, PU_OUTPUT, -1, -1, -1},
        {"Input dB", "Ratio", "Output dB", "", "", ""}},
    {"TIMING", 1, {PU_ATTACK, PU_RELEASE, PU_ATMOD, -1, -1, -1},
        {"Attack", "Release", "AtMod", "", "", ""}},
    {"COLOUR", 2, {PU_SAT, PU_DET_HP, PU_MIX, -1, -1, -1},
        {"Sat", "Det HP 110", "Mix %", "", "", ""}}
};
static constexpr FlavorBank dist_banks[] = {
    {"COMPRESS", 0, {DS_INPUT, DS_RATIO, DS_OUTPUT, -1, -1, -1}, {"Input", "Ratio", "Output", "", "", ""}},
    {"TIMING", 1, {DS_ATTACK, DS_RELEASE, DS_DETECTOR, -1, -1, -1}, {"Attack", "Release", "Detector", "", "", ""}},
    {"AUDIO", 2, {DS_AUDIO, DS_BRIT, DS_MIX, -1, -1, -1}, {"Audio", "British", "Mix %", "", "", ""}}
};
static constexpr FlavorBank tube_banks[] = {
    {"TUBE", 0, {TS_DRIVE, TS_HPF, TS_OUTPUT, -1, -1, -1}, {"Drive", "HPF Hz", "Output dB", "", "", ""}},
    {"OPTO COMP", 1, {TS_THRESHOLD, TS_RATIO, TS_ATTACK, -1, -1, -1}, {"Threshold dB", "Ratio", "Attack ms", "", "", ""}},
    {"COMP TIME", 2, {TS_RELEASE, TS_ORDER, TS_SC, -1, -1, -1}, {"Release ms", "EQ First", "SC EQ", "", "", ""}},
    {"BASS", 3, {TS_BASS_HZ, TS_BASS_DB, -1, -1, -1, -1}, {"Bass Hz", "Bass dB", "", "", "", ""}},
    {"LOW MID", 4, {TS_LMID_HZ, TS_LMID_DB, TS_Q, -1, -1, -1}, {"Low Mid Hz", "Low Mid dB", "High Q", "", "", ""}},
    {"HIGH MID", 5, {TS_HMID_HZ, TS_HMID_DB, TS_Q, -1, -1, -1}, {"High Mid Hz", "High Mid dB", "High Q", "", "", ""}},
    {"TREBLE", 6, {TS_TREBLE_HZ, TS_TREBLE_DB, -1, -1, -1, -1}, {"Treble Hz", "Treble dB", "", "", "", ""}}
};
static constexpr FlavorBank heat_banks[] = {
    {"CHARACTER", 0, {HE_CIRCUIT, HE_DRIVE, HE_WET, -1, -1, -1}, {"Circuit", "Drive", "Wet dB", "", "", ""}},
    {"FILTER", 1, {HE_FTYPE, HE_FREQ, HE_RESO, -1, -1, -1}, {"Filter Type", "Frequency", "Resonance", "", "", ""}},
    {"DIRT + EQ", 2, {HE_DIRT, HE_EQ_LO, HE_EQ_HI, -1, -1, -1}, {"Dirt", "EQ Low", "EQ High", "", "", ""}},
    {"ENVELOPE", 3, {HE_ENV_DEPTH, HE_ENV_ATK, HE_ENV_REL, -1, -1, -1}, {"Env > Freq", "Attack", "Release", "", "", ""}},
    {"LFO", 4, {HE_LFO_DEPTH, HE_LFO_SPEED, HE_LFO_WAVE, -1, -1, -1}, {"LFO > Freq", "Speed", "Wave", "", "", ""}},
    {"OUTPUT", 5, {HE_FREQ_PAN, HE_MIX, HE_VOLUME, -1, -1, -1}, {"Freq Pan", "Mix %", "Volume dB", "", "", ""}}
};
static constexpr FlavorBank eq_global[] = {
    {"GLOBAL",0,{EQ_INPUT,EQ_OUTPUT,EQ_MIDSIDE,-1,-1,-1},{"Input dB","Output dB","Mid/Side","","",""}},
    {"ROUTING",1,{EQ_PARALLEL,EQ_BYPASS,-1,-1,-1,-1},{"Parallel","Bypass","","","",""}}
};
static constexpr FlavorBank eq_band[] = {
    {"SHAPE",0,{EB_FREQ,EB_GAIN,EB_Q,-1,-1,-1},{"Freq Hz","Gain dB","Q","","",""}},
    {"MODE",1,{EB_TYPE,EB_ON,EB_BYPASS,-1,-1,-1},{"Type","Band On","Band Bypass","","",""}},
    {"FILTER",2,{EB_FAMILY,EB_SLOPE,EB_BELL,-1,-1,-1},{"Filter Family","Slope dB/oct","Bell Shape","","",""}},
    {"STEREO",3,{EB_STEREO,-1,-1,-1,-1,-1},{"Stereo Pos","","","","",""}}
};
/* One atomic word keeps band and page coherent through the overlay callbacks. */
static unsigned eq_selection = 4; // band 1, page 0
static int (*next_cc)(int,int);

static constexpr unsigned bus_count = sizeof bus_banks / sizeof *bus_banks;
static unsigned bus_bank;

static constexpr unsigned heat_count = sizeof heat_banks / sizeof *heat_banks;
static unsigned heat_bank;

static constexpr unsigned pump_count = sizeof pump_banks / sizeof *pump_banks;
static constexpr unsigned dist_count = sizeof dist_banks / sizeof *dist_banks;
static constexpr unsigned tube_count = sizeof tube_banks / sizeof *tube_banks;
static unsigned tube_bank;
static unsigned dist_bank;

static unsigned pump_bank;

static constexpr unsigned xsat_count = sizeof xsat_banks / sizeof *xsat_banks;
static unsigned xsat_bank;

static constexpr unsigned channel_count = sizeof channel_banks / sizeof *channel_banks;
static unsigned channel_bank;

static constexpr unsigned bank_count = sizeof banks / sizeof *banks;
static unsigned bank;
static uintptr_t qol_base;
static int (*next_note)(int, int, int);
static int (*next_map)(void *, int, void *);
static int (*question_active)();
static void (*refresh)();

static int state(uintptr_t offset)
{
    return __atomic_load_n(reinterpret_cast<const int *>(qol_base + offset), __ATOMIC_ACQUIRE);
}

static unsigned insert_open()
{
    return qol_base && state(0x1632688) && state(0x16328dc) ? state(0x16328f4) : 0;
}

extern "C" const char *hs_xsat_bank(int *section)
{
    if (insert_open() != HS_XSAT_ID) return nullptr;
    const auto &page = xsat_banks[__atomic_load_n(&xsat_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_bus_bank(int *section)
{
    if (insert_open() != HS_BUS_ID) return nullptr;
    const auto &page = bus_banks[__atomic_load_n(&bus_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_pump_bank(int *section)
{
    if (insert_open() != HS_PUMP_ID) return nullptr;
    const auto &page = pump_banks[__atomic_load_n(&pump_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_distressor_bank(int *section)
{
    if (insert_open() != HS_DISTRESSOR_ID) return nullptr;
    const auto &page = dist_banks[__atomic_load_n(&dist_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_tube_strip_bank(int *section)
{
    if (insert_open() != HS_TUBE_STRIP_ID) return nullptr;
    const auto &page = tube_banks[__atomic_load_n(&tube_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_heat_bank(int *section)
{
    if (insert_open() != HS_HEAT_ID) return nullptr;
    const auto &page = heat_banks[__atomic_load_n(&heat_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" int hs_eq2_selection(int *page)
{
    if (insert_open() != HS_EQ2_ID) return -1;
    unsigned sel=__atomic_load_n(&eq_selection,__ATOMIC_RELAXED);
    *page=sel%4;
    return sel/4;
}

static bool flavor_open() { return insert_open() == flavor_uid; }

extern "C" const char *hs_channel_bank(int *module)
{
    if (insert_open() != HS_CHANNEL_ID) return nullptr;
    const auto &page = channel_banks[__atomic_load_n(&channel_bank, __ATOMIC_RELAXED)];
    *module = page.module;
    return page.title;
}

extern "C" const char *hs_flavor_bank(int *module)
{
    if (!flavor_open()) return nullptr;
    unsigned index = __atomic_load_n(&bank, __ATOMIC_RELAXED);
    *module = banks[index].module;
    return banks[index].title;
}

extern "C" int hs_flavor_power_module()
{
    if (!flavor_open()) return -1;
    const auto &page = banks[__atomic_load_n(&bank, __ATOMIC_RELAXED)];
    return page.module >= 0 && page.parameters[0] == page.module ? page.module : -1;
}

/* Header label over B2: page index and count of the open HS editor, or -1 when none is open. */
extern "C" int hs_bank_position(int *count)
{
    unsigned uid = insert_open();
    if (uid == HS_EQ2_ID) {
        unsigned sel = __atomic_load_n(&eq_selection, __ATOMIC_RELAXED);
        *count = sel/4 ? 4 : 2;
        return static_cast<int>(sel%4);
    }
    const unsigned *selection = uid == flavor_uid ? &bank : uid == HS_XSAT_ID ? &xsat_bank : uid == HS_BUS_ID ? &bus_bank
        : uid == HS_PUMP_ID ? &pump_bank : uid == HS_DISTRESSOR_ID ? &dist_bank : uid == HS_TUBE_STRIP_ID ? &tube_bank : uid == HS_HEAT_ID ? &heat_bank
        : uid == HS_CHANNEL_ID ? &channel_bank : nullptr;
    if (!selection) return -1;
    *count = static_cast<int>(uid == flavor_uid ? bank_count : uid == HS_XSAT_ID ? xsat_count : uid == HS_BUS_ID ? bus_count
        : uid == HS_PUMP_ID ? pump_count : uid == HS_DISTRESSOR_ID ? dist_count : uid == HS_TUBE_STRIP_ID ? tube_count : uid == HS_HEAT_ID ? heat_count : channel_count);
    return static_cast<int>(__atomic_load_n(selection, __ATOMIC_RELAXED));
}

__attribute__((constructor)) static void init_editor()
{
    next_cc = reinterpret_cast<decltype(next_cc)>(dlsym(RTLD_NEXT, "padmix_cc"));
    next_note = reinterpret_cast<decltype(next_note)>(dlsym(RTLD_NEXT, "padmix_note"));
    next_map = reinterpret_cast<decltype(next_map)>(dlsym(RTLD_NEXT, "fxmap_get_row"));
    question_active = reinterpret_cast<decltype(question_active)>(dlsym(RTLD_NEXT, "context_question_active"));
    refresh = reinterpret_cast<decltype(refresh)>(dlsym(RTLD_NEXT, "padmix_force"));
    Dl_info info;
    if (next_note && next_map && question_active && refresh &&
        dladdr(reinterpret_cast<void *>(refresh), &info) &&
        reinterpret_cast<uintptr_t>(refresh) - reinterpret_cast<uintptr_t>(info.dli_fbase) == 0xa3300)
        qol_base = reinterpret_cast<uintptr_t>(info.dli_fbase);
}

extern "C" int padmix_cc(int cc,int value)
{
    if (cc==100 && insert_open()==HS_EQ2_ID && !question_active()) {
        int amount=value >= 64 ? value-128 : value;
        if (amount) {
            unsigned sel=__atomic_load_n(&eq_selection,__ATOMIC_RELAXED);
            int target=static_cast<int>(sel/4)+amount;
            target=target<0 ? 0 : target>EQ_BANDS ? EQ_BANDS : target;
            if (target!=static_cast<int>(sel/4)) {
                unsigned page=target && sel/4 ? sel%4 : 0;
                __atomic_store_n(&eq_selection,4*target+page,__ATOMIC_RELAXED);
                refresh();
            }
        }
        return 1;
    }
    return next_cc ? next_cc(cc,value) : 0;
}

extern "C" int padmix_note(int note, int value, int shift)
{
    unsigned uid = insert_open();
    if (note == 13 && uid == HS_EQ2_ID && !question_active()) {
        if (value>0) {
            unsigned sel=__atomic_load_n(&eq_selection,__ATOMIC_RELAXED);
            unsigned count=sel/4 ? 4 : 2;
            unsigned page=(sel%4+(shift ? count-1 : 1))%count;
            __atomic_store_n(&eq_selection,sel/4*4+page,__ATOMIC_RELAXED);
            refresh();
        }
        return 1;
    }
    if (note == 13 && (uid == flavor_uid || uid == HS_CHANNEL_ID || uid == HS_XSAT_ID || uid == HS_BUS_ID || uid == HS_PUMP_ID || uid == HS_HEAT_ID || uid == HS_DISTRESSOR_ID || uid == HS_TUBE_STRIP_ID) && !question_active()) {
        unsigned *selection = uid == flavor_uid ? &bank : uid == HS_XSAT_ID ? &xsat_bank : uid == HS_BUS_ID ? &bus_bank
            : uid == HS_PUMP_ID ? &pump_bank : uid == HS_DISTRESSOR_ID ? &dist_bank : uid == HS_TUBE_STRIP_ID ? &tube_bank : uid == HS_HEAT_ID ? &heat_bank : &channel_bank;
        unsigned count = uid == flavor_uid ? bank_count : uid == HS_XSAT_ID ? xsat_count : uid == HS_BUS_ID ? bus_count
            : uid == HS_PUMP_ID ? pump_count : uid == HS_DISTRESSOR_ID ? dist_count : uid == HS_TUBE_STRIP_ID ? tube_count : uid == HS_HEAT_ID ? heat_count : channel_count;
        if (value > 0) {
            unsigned previous = __atomic_load_n(selection, __ATOMIC_RELAXED);
            unsigned page = (previous + (shift ? count - 1 : 1)) % count;
            __atomic_store_n(selection, page, __ATOMIC_RELAXED);
            refresh();
        }
        return 1;
    }
    return next_note ? next_note(note, value, shift) : 0;
}

extern "C" int fxmap_get_row(void *context, int id, void *output)
{
    unsigned uid = insert_open();
    if (!output || (uid != flavor_uid && uid != HS_CHANNEL_ID && uid != HS_XSAT_ID && uid != HS_BUS_ID && uid != HS_EQ2_ID && uid != HS_PUMP_ID && uid != HS_HEAT_ID && uid != HS_DISTRESSOR_ID && uid != HS_TUBE_STRIP_ID) || id != -1)
        return next_map ? next_map(context, id, output) : 0;
    struct MapRow {
        char name[24];
        int parameters[6];
        char labels[6][16];
    };
    static_assert(sizeof(MapRow) == 144, "SampQOL effect map");
    auto *row = static_cast<MapRow *>(output);
    if (uid == HS_EQ2_ID) {
        unsigned sel=__atomic_load_n(&eq_selection,__ATOMIC_RELAXED);
        unsigned band=sel/4;
        const auto &page=band ? eq_band[sel%4] : eq_global[sel%4];
        memset(row,0,sizeof *row);
        /* Band in the title: bands 5-24 share default settings, so without it their frames are
           pixel-identical and SampQOL's duplicate skip never redraws the band change. */
        snprintf(row->name,sizeof row->name,band ? "HS Para EQ BAND %02u" : "HS Para EQ GLOBAL",band);
        memcpy(row->labels,page.labels,sizeof row->labels);
        for (int i=0;i<6;++i)
            row->parameters[i]=band && page.parameters[i]>=0
                ? EQ_FIRST+(band-1)*EB_COUNT+page.parameters[i] : page.parameters[i];
        return 1;
    }
    const auto &page = uid == flavor_uid
        ? banks[__atomic_load_n(&bank, __ATOMIC_RELAXED)]
        : uid == HS_XSAT_ID ? xsat_banks[__atomic_load_n(&xsat_bank, __ATOMIC_RELAXED)]
        : uid == HS_BUS_ID ? bus_banks[__atomic_load_n(&bus_bank, __ATOMIC_RELAXED)]
        : uid == HS_PUMP_ID ? pump_banks[__atomic_load_n(&pump_bank, __ATOMIC_RELAXED)]
        : uid == HS_DISTRESSOR_ID ? dist_banks[__atomic_load_n(&dist_bank, __ATOMIC_RELAXED)]
        : uid == HS_TUBE_STRIP_ID ? tube_banks[__atomic_load_n(&tube_bank, __ATOMIC_RELAXED)]
        : uid == HS_HEAT_ID ? heat_banks[__atomic_load_n(&heat_bank, __ATOMIC_RELAXED)]
        : channel_banks[__atomic_load_n(&channel_bank, __ATOMIC_RELAXED)];
    memset(row, 0, sizeof *row);
    if (uid == flavor_uid) memcpy(row->name, flavor_name, sizeof flavor_name);
    else if (uid == HS_XSAT_ID) memcpy(row->name, "HS Drive Sat", sizeof "HS Drive Sat");
    else if (uid == HS_BUS_ID) memcpy(row->name, "HS Bus Compressor", sizeof "HS Bus Compressor");
    else if (uid == HS_PUMP_ID) memcpy(row->name, "HS Pump", sizeof "HS Pump");
    else if (uid == HS_DISTRESSOR_ID) memcpy(row->name, "HS Crush", sizeof "HS Crush");
    else if (uid == HS_TUBE_STRIP_ID) memcpy(row->name, "HS Tube Strip", sizeof "HS Tube Strip");
    else if (uid == HS_HEAT_ID) memcpy(row->name, "HS Heat", sizeof "HS Heat");
    else memcpy(row->name, "HS Channel Strip", sizeof "HS Channel Strip");
    memcpy(row->parameters, page.parameters, sizeof row->parameters);
    memcpy(row->labels, page.labels, sizeof row->labels);
    return 1;
}
