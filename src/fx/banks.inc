struct FlavorBank {
    const char *title;
    int module;
    int parameters[6];
    char labels[6][16];
    char units[6][4];
};

static constexpr FlavorBank banks[] = {
    {"GLOBAL", -1, {6, 10, 7, 8, 9, -1}, {"Intensity", "Width", "Gain", "High-Pass", "Low-Pass", ""}, {}},
    {"PITCH", 0, {0, 11, 12, 13, 16, 14}, {"Power", "Depth", "Vary", "Fast Rate", "Slow Rate", "Drift"}, {}},
    {"PITCH X-MOD", 0, {17, 18, 19, 15, 20, -1}, {"X-Mod", "Type", "Mix", "Stereo", "Fast/Slow", ""}, {}},
    {"DISTORTION", 1, {1, 21, 23, 22, 24, 27}, {"Power", "Depth", "Model", "Vary", "Mix", "Smooth"}, {}},
    {"DIST INPUT", 1, {25, 26, -1, -1, -1, -1}, {"Input HP", "Input LP", "", "", "", ""}, {}},
    {"DIGITAL", 2, {2, 30, 31, 28, 29, 36}, {"Power", "Bits", "Rate", "Depth", "Vary", "Smooth"}, {}},
    {"DIGITAL SHAPE", 2, {32, 33, 37, 34, 35, 38}, {"Glitch", "Length", "Envelope", "Input HP", "Input LP", "Stereo"}, {}},
    {"VINYL", 3, {3, 39, 41, 40, 42, 44}, {"Power", "Depth", "Crackle", "Vary", "Clicks", "Pops"}, {}},
    {"VINYL NOISE", 3, {43, 45, 46, 47, 48, 49}, {"Rate", "Rumble", "Noise", "Tone", "Stereo", "Hum"}, {}},
    {"VINYL ENVELOPE", 3, {50, 51, -1, -1, -1, -1}, {"Envelope", "Release", "", "", "", ""}, {}},
    {"VOLUME", 4, {4, 52, 60, 53, 54, 55}, {"Power", "Depth", "Dropouts", "Vary", "Fast Rate", "Slow Rate"}, {}},
    {"VOLUME SHAPE", 4, {57, 58, 59, 56, -1, -1}, {"Fast Drift", "Slow Drift", "Stereo", "Fast/Slow", "", ""}, {}},
    {"TIMBRE", 5, {5, 61, 63, 62, -1, -1}, {"Power", "Depth", "Profile", "Vary", "", ""}, {}}
};
/* Labels use stock wording: plain words, no units. The unit (last field) is drawn after the value
   by hs-fx (hs_knob_unit), as stock shows "-6.0 dB" under "Threshold". Knobs whose value hs-fx
   draws itself (ratios, switches, times with units) have no unit here. */
static constexpr FlavorBank channel_banks[] = {
    {"INPUT / OUTPUT", 0, {CH_INPUT, CH_DRIVE, CH_OUTPUT, -1, -1, -1},
        {"Input", "Drive", "Output", "", "", ""}, {"DB", "%", "DB"}},
    {"242 BLACK FILTERS", 1, {CH_HP, CH_LP, CH_EQ_FIRST, -1, -1, -1},
        {"High-Pass", "Low-Pass", "EQ First", "", "", ""}, {"HZ", "HZ"}},
    {"LOW EQ", 2, {CH_LF_DB, CH_LF_HZ, CH_LF_BELL, -1, -1, -1},
        {"Gain", "Frequency", "Bell", "", "", ""}, {"DB", "HZ"}},
    {"LOW MID EQ", 3, {CH_LM_DB, CH_LM_HZ, CH_LM_Q, -1, -1, -1},
        {"Gain", "Frequency", "Q", "", "", ""}, {"DB", "HZ"}},
    {"HIGH MID EQ", 4, {CH_HM_DB, CH_HM_HZ, CH_HM_Q, -1, -1, -1},
        {"Gain", "Frequency", "Q", "", "", ""}, {"DB", "HZ"}},
    {"HIGH EQ", 5, {CH_HF_DB, CH_HF_HZ, CH_HF_BELL, -1, -1, -1},
        {"Gain", "Frequency", "Bell", "", "", ""}, {"DB", "HZ"}},
    {"COMPRESSOR", 6, {CH_COMP_THRESHOLD, CH_COMP_RATIO, CH_MAKEUP,
                        CH_COMP_FAST, CH_COMP_RELEASE, CH_COMP_HARD},
        {"Threshold", "Ratio", "Make-Up", "Fast Attack", "Release", "Hard Knee"}, {"DB", "", "DB", "", "MS"}},
    {"GATE / EXPAND", 7, {CH_GATE_THRESHOLD, CH_GATE_RANGE, CH_GATE_RELEASE,
                         CH_GATE_FAST, CH_GATE_EXPAND, -1},
        {"Threshold", "Range", "Release", "Fast Attack", "Expand", ""}, {"DB", "DB", "MS"}},
    {"DE-ESSER", 8, {CH_DS_FREQ, CH_DS_THRESHOLD, CH_DS_RANGE,
                     CH_DS_MODE, -1, -1},
        {"Frequency", "Threshold", "Range", "Mode", "", ""}, {"HZ", "DB", "DB"}},
    HS_MOD_BANKS(CH_MOD, 0)
};
/* No Bypass knob on any HS page: SampQOL's B3 is the one Bypass (the old ports are ignored). */
static constexpr FlavorBank xsat_banks[] = {
    {"DRIVE", 0, {XS_DRIVE, XS_SHAPE, XS_DEPTH, -1, -1, -1},
        {"Drive", "Shape", "Depth", "", "", ""}, {"DB", "", "%"}},
    {"HARMONICS", 1, {XS_HARMONICS, XS_MIX, XS_HEADROOM, -1, -1, -1},
        {"Harmonics", "Mix", "Headroom +6", "", "", ""}, {"%", "%"}},
    {"INPUT / OUTPUT", 2, {XS_INPUT, XS_OUTPUT, -1, -1, -1, -1},
        {"Input", "Output", "", "", "", ""}, {"DB", "DB"}},
    HS_MOD_BANKS(XS_MOD, 0)
};
static constexpr FlavorBank bus_banks[] = {
    {"COMPRESSION", 0, {BUS_THRESHOLD, BUS_RATIO, BUS_MAKEUP, -1, -1, -1},
        {"Threshold", "Ratio", "Make-Up", "", "", ""}, {"DB", "", "DB"}},
    {"TIMING", 1, {BUS_ATTACK, BUS_RELEASE, BUS_HPF, -1, -1, -1},
        {"Attack", "Release", "Sidechain HP", "", "", ""}, {"", "", "HZ"}},
    {"PARALLEL", 2, {BUS_MIX, -1, -1, -1, -1, -1},
        {"Mix", "", "", "", "", ""}, {"%"}},
    HS_MOD_BANKS(BUS_MOD, 0)
};
static constexpr FlavorBank pump_banks[] = {
    {"COMPRESS", 0, {PU_INPUT, PU_RATIO, PU_OUTPUT, -1, -1, -1},
        {"Input", "Ratio", "Output", "", "", ""}, {"DB", "", "DB"}},
    {"TIMING", 1, {PU_ATTACK, PU_RELEASE, PU_ATMOD, -1, -1, -1},
        {"Attack", "Release", "Attack Mod", "", "", ""}, {}},
    {"COLOUR", 2, {PU_SAT, PU_DET_HP, PU_MIX, -1, -1, -1},
        {"Saturation", "Detector HP", "Mix", "", "", ""}, {"", "", "%"}},
    {"KEY", 3, {PU_KEY, PU_SEND, -1, -1, -1, -1},
        {"Key In", "Send", "", "", "", ""}, {}},
    HS_MOD_BANKS(PU_MOD, 0)
};
static constexpr FlavorBank dist_banks[] = {
    {"COMPRESS", 0, {DS_INPUT, DS_RATIO, DS_OUTPUT, -1, -1, -1}, {"Input", "Ratio", "Output", "", "", ""}, {}},
    {"TIMING", 1, {DS_ATTACK, DS_RELEASE, DS_DETECTOR, -1, -1, -1}, {"Attack", "Release", "Detector", "", "", ""}, {}},
    {"AUDIO", 2, {DS_AUDIO, DS_BRIT, DS_MIX, -1, -1, -1}, {"Audio", "British", "Mix", "", "", ""}, {"", "", "%"}},
    {"KEY", 3, {DS_KEY, DS_SEND, -1, -1, -1, -1}, {"Key In", "Send", "", "", "", ""}, {}},
    HS_MOD_BANKS(DS_MOD, 0)
};
static constexpr FlavorBank tube_banks[] = {
    {"TUBE", 0, {TS_DRIVE, TS_HPF, TS_OUTPUT, -1, -1, -1}, {"Drive", "High-Pass", "Output", "", "", ""}, {}},
    {"OPTO COMP", 1, {TS_THRESHOLD, TS_RATIO, TS_ATTACK, -1, -1, -1}, {"Threshold", "Ratio", "Attack", "", "", ""}, {}},
    {"COMP TIME", 2, {TS_RELEASE, TS_ORDER, TS_SC, -1, -1, -1}, {"Release", "EQ First", "Sidechain EQ", "", "", ""}, {}},
    {"BASS", 3, {TS_BASS_HZ, TS_BASS_DB, -1, -1, -1, -1}, {"Frequency", "Gain", "", "", "", ""}, {}},
    {"LOW MID", 4, {TS_LMID_HZ, TS_LMID_DB, TS_Q, -1, -1, -1}, {"Frequency", "Gain", "High Q", "", "", ""}, {}},
    {"HIGH MID", 5, {TS_HMID_HZ, TS_HMID_DB, TS_Q, -1, -1, -1}, {"Frequency", "Gain", "High Q", "", "", ""}, {}},
    {"TREBLE", 6, {TS_TREBLE_HZ, TS_TREBLE_DB, -1, -1, -1, -1}, {"Frequency", "Gain", "", "", "", ""}, {}},
    HS_MOD_BANKS(TS_MOD, 0)
};
static constexpr FlavorBank heat_banks[] = {
    {"CHARACTER", 0, {HE_CIRCUIT, HE_DRIVE, HE_WET, -1, -1, -1}, {"Circuit", "Drive", "Wet", "", "", ""}, {"", "", "DB"}},
    {"FILTER", 1, {HE_FTYPE, HE_FREQ, HE_RESO, -1, -1, -1}, {"Filter Type", "Frequency", "Resonance", "", "", ""}, {}},
    {"DIRT + EQ", 2, {HE_DIRT, HE_EQ_LO, HE_EQ_HI, -1, -1, -1}, {"Dirt", "EQ Low", "EQ High", "", "", ""}, {}},
    {"ENVELOPE", 3, {HE_ENV_DEPTH, HE_ENV_ATK, HE_ENV_REL, -1, -1, -1}, {"Env Depth", "Attack", "Release", "", "", ""}, {}},
    {"LFO", 4, {HE_LFO_DEPTH, HE_LFO_SPEED, HE_LFO_WAVE, -1, -1, -1}, {"LFO Depth", "Speed", "Wave", "", "", ""}, {}},
    {"OUTPUT", 5, {HE_FREQ_PAN, HE_MIX, HE_VOLUME, -1, -1, -1}, {"Freq Pan", "Mix", "Volume", "", "", ""}, {"", "%", "DB"}},
    HS_MOD_BANKS(HE_MOD, 0)
};
static constexpr FlavorBank eq_global[] = {
    {"GLOBAL",0,{EQ_INPUT,EQ_OUTPUT,EQ_MIDSIDE,-1,-1,-1},{"Input","Output","Mid/Side","","",""},{"DB","DB"}},
    {"ROUTING",1,{EQ_PARALLEL,-1,-1,-1,-1,-1},{"Parallel","","","","",""}, {}}
};
static constexpr FlavorBank eq_band[] = {
    {"SHAPE",0,{EB_FREQ,EB_GAIN,EB_Q,-1,-1,-1},{"Frequency","Gain","Q","","",""},{"HZ","DB"}},
    {"MODE",1,{EB_TYPE,EB_ON,EB_BYPASS,-1,-1,-1},{"Type","Band On","Band Bypass","","",""}, {}},
    {"FILTER",2,{EB_FAMILY,EB_SLOPE,EB_BELL,-1,-1,-1},{"Filter Family","Slope","Bell Shape","","",""}, {}},
    {"STEREO",3,{EB_STEREO,-1,-1,-1,-1,-1},{"Position","","","","",""}, {}}
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

static void follow_instance();

static unsigned insert_open()
{
    unsigned uid = qol_base && state(0x1632688) && state(0x16328dc) ? state(0x16328f4) : 0;
    if (uid) follow_instance();
    return uid;
}

/* SampQOL 0.2.0: insert editor at qol+0x16333a0, open insert at +0xd0 (same read as hs_eq2_view.h). */
static const char *open_insert()
{
    if (!qol_base) return nullptr;
    auto *editor = *reinterpret_cast<char *const *>(qol_base + 0x16333a0);
    return editor ? *reinterpret_cast<char *const *>(editor + 0xd0) : nullptr;
}

static unsigned *paged(unsigned uid, unsigned *count)
{
    if (uid == HS_XSAT_ID) { *count = xsat_count; return &xsat_bank; }
    if (uid == HS_BUS_ID) { *count = bus_count; return &bus_bank; }
    if (uid == HS_PUMP_ID) { *count = pump_count; return &pump_bank; }
    if (uid == HS_DISTRESSOR_ID) { *count = dist_count; return &dist_bank; }
    if (uid == HS_TUBE_STRIP_ID) { *count = tube_count; return &tube_bank; }
    if (uid == HS_HEAT_ID) { *count = heat_count; return &heat_bank; }
    if (uid == HS_CHANNEL_ID) { *count = channel_count; return &channel_bank; }
    if (uid == flavor_uid) { *count = bank_count; return &bank; }
    return nullptr;
}

/* MOD knobs last seen on the open insert (hs_mod_view), -1 = not yet. */
static int mod_seen_source = -1, mod_seen_target = -1;

/* Page state is one word per effect type. When the editor opens a different insert (another slot,
   track or a second copy of the effect), start it on page 1 and band 1, as stock opens a screen.
   ponytail: keyed on SampQOL's insert object; reopening the same insert keeps its page. */
static void follow_instance()
{
    static const void *last;
    const void *now = open_insert();
    if (!now || __atomic_exchange_n(&last, now, __ATOMIC_ACQ_REL) == now) return;
    unsigned *pages[] = {&xsat_bank, &bus_bank, &pump_bank, &dist_bank, &tube_bank, &heat_bank, &channel_bank, &bank};
    for (unsigned *s : pages) __atomic_store_n(s, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&eq_selection, 4u, __ATOMIC_RELAXED);
    mod_seen_source = mod_seen_target = -1;     /* hs_mod_view: MOD knobs not seen on this insert */
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

/* Flavor: position of the page inside its module tab, for the dots under the tab (stock idiom). */
extern "C" int hs_flavor_subpage(int *count)
{
    if (!flavor_open()) return -1;
    unsigned at = __atomic_load_n(&bank, __ATOMIC_RELAXED), first = at, end = at;
    while (first > 0 && banks[first - 1].module == banks[at].module) --first;
    while (end + 1 < bank_count && banks[end + 1].module == banks[at].module) ++end;
    *count = static_cast<int>(end - first + 1);
    return static_cast<int>(at - first);
}

/* Unit hs-fx draws after knob (0-5) of the open HS page's value, or nullptr. */
extern "C" const char *hs_knob_unit(int knob)
{
    unsigned uid = insert_open(), count;
    if (knob < 0 || knob > 5) return nullptr;
    const FlavorBank *page = nullptr;
    if (uid == HS_EQ2_ID) {
        unsigned sel = __atomic_load_n(&eq_selection, __ATOMIC_RELAXED);
        page = sel/4 ? &eq_band[sel%4] : &eq_global[sel%4 % 2];
    } else if (const unsigned *s = uid != flavor_uid ? paged(uid, &count) : nullptr) {
        unsigned at = __atomic_load_n(s, __ATOMIC_RELAXED);
        const FlavorBank *table = uid == HS_XSAT_ID ? xsat_banks : uid == HS_BUS_ID ? bus_banks
            : uid == HS_PUMP_ID ? pump_banks : uid == HS_DISTRESSOR_ID ? dist_banks
            : uid == HS_TUBE_STRIP_ID ? tube_banks : uid == HS_HEAT_ID ? heat_banks : channel_banks;
        if (at < count) page = &table[at];
    }
    return page && page->units[knob][0] ? page->units[knob] : nullptr;
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

/* MOD pages (hs_mod.h, the last two pages of every paged HS effect but the flavor) for hs-fx's panel:
   1 = MOD, 2 = MOD TIME, 0 = not on one. knobs = the page's knob positions (0-72, -1 unused) as hs-fx
   sees them. On MOD, Source and Target are read from those knobs and remembered for this insert, so
   MOD TIME can show them too; *source is -1 until the MOD page has been seen on this insert.
   ponytail: the kit has no instance registry to read the live ports, so this follows the knobs. */
extern "C" int hs_mod_view(const int *knobs, int *source, const char **target_page, const char **target)
{
    unsigned uid = insert_open(), count;
    const FlavorBank *pages = uid == HS_XSAT_ID ? xsat_banks : uid == HS_BUS_ID ? bus_banks
        : uid == HS_PUMP_ID ? pump_banks : uid == HS_DISTRESSOR_ID ? dist_banks
        : uid == HS_TUBE_STRIP_ID ? tube_banks : uid == HS_HEAT_ID ? heat_banks
        : uid == HS_CHANNEL_ID ? channel_banks : nullptr;
    unsigned *at = pages ? paged(uid, &count) : nullptr;
    unsigned page = at ? __atomic_load_n(at, __ATOMIC_RELAXED) : 0;
    if (!at || count < 3 || page + 2 < count || page >= count) return 0;
    int base = pages[count - 2].parameters[0] - HS_MOD_SRC;   /* = the effect's own control count */
    if (page + 2 == count && knobs && knobs[0] >= 0 && knobs[1] >= 0) {
        mod_seen_source = (knobs[0]*4 + 36)/73;   /* Off, A-D: integer knob steps, as the panels read them */
        mod_seen_target = (knobs[1]*(base - 1) + 36)/73;
    }
    *source = mod_seen_source;
    *target_page = *target = nullptr;
    for (unsigned i = 0; mod_seen_target >= 0 && i + 2 < count && !*target; ++i)
        for (int k = 0; k < 6; ++k)
            if (pages[i].parameters[k] == mod_seen_target) { *target_page = pages[i].title; *target = pages[i].labels[k]; break; }
    return static_cast<int>(page + 3 - count);
}
