#include "bank_tables.inc"   /* the page tables, shared with the CLAP faceplate (src/clap/hs_clap_panel.c) */
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

#include "presets.inc"

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
    __atomic_store_n(&eq_preset, 0u, __ATOMIC_RELAXED);
    mod_seen_source = mod_seen_target = -1;     /* hs_mod_view: MOD knobs not seen on this insert */
}

extern "C" const char *hs_xsat_bank(int *section)
{
    if (insert_open() != HS_XSAT_ID) return nullptr;
    if (__atomic_load_n(&xsat_bank, __ATOMIC_RELAXED) >= xsat_count) { *section = -1; return "PRESETS"; }
    const auto &page = xsat_banks[__atomic_load_n(&xsat_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_bus_bank(int *section)
{
    if (insert_open() != HS_BUS_ID) return nullptr;
    if (__atomic_load_n(&bus_bank, __ATOMIC_RELAXED) >= bus_count) { *section = -1; return "PRESETS"; }
    const auto &page = bus_banks[__atomic_load_n(&bus_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_pump_bank(int *section)
{
    if (insert_open() != HS_PUMP_ID) return nullptr;
    if (__atomic_load_n(&pump_bank, __ATOMIC_RELAXED) >= pump_count) { *section = -1; return "PRESETS"; }
    const auto &page = pump_banks[__atomic_load_n(&pump_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_distressor_bank(int *section)
{
    if (insert_open() != HS_DISTRESSOR_ID) return nullptr;
    if (__atomic_load_n(&dist_bank, __ATOMIC_RELAXED) >= dist_count) { *section = -1; return "PRESETS"; }
    const auto &page = dist_banks[__atomic_load_n(&dist_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_tube_strip_bank(int *section)
{
    if (insert_open() != HS_TUBE_STRIP_ID) return nullptr;
    if (__atomic_load_n(&tube_bank, __ATOMIC_RELAXED) >= tube_count) { *section = -1; return "PRESETS"; }
    const auto &page = tube_banks[__atomic_load_n(&tube_bank, __ATOMIC_RELAXED)];
    *section = page.module;
    return page.title;
}

extern "C" const char *hs_heat_bank(int *section)
{
    if (insert_open() != HS_HEAT_ID) return nullptr;
    if (__atomic_load_n(&heat_bank, __ATOMIC_RELAXED) >= heat_count) { *section = -1; return "PRESETS"; }
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
    if (__atomic_load_n(&channel_bank, __ATOMIC_RELAXED) >= channel_count) { *module = -1; return "PRESETS"; }
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

/* The open HS page (not the flavor's), or nullptr. */
static const FlavorBank *open_page()
{
    unsigned uid = insert_open(), count;
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
    return page;
}

/* Unit hs-fx draws after knob (0-5) of the open HS page's value, or nullptr. */
extern "C" const char *hs_knob_unit(int knob)
{
    const FlavorBank *page = knob >= 0 && knob <= 5 ? open_page() : nullptr;
    return page && page->units[knob][0] ? page->units[knob] : nullptr;
}

/* Parameter under knob (0-5) of the open HS page, numbered as a MOD Target is, or -1. hs-fx turns
   that knob when the effect's MOD is moving the parameter. EQ bands are not offset: no MOD there. */
extern "C" int hs_knob_param(int knob)
{
    const FlavorBank *page = knob >= 0 && knob <= 5 ? open_page() : nullptr;
    return page ? page->parameters[knob] : -1;
}

/* Header label over B2: page index and count of the open HS editor, or -1 when none is open. */
extern "C" int hs_bank_position(int *count)
{
    unsigned uid = insert_open();
    if (uid == HS_EQ2_ID) {
        unsigned sel = __atomic_load_n(&eq_selection, __ATOMIC_RELAXED);
        *count = sel/4 ? 4 : 2;
        return preset_open() ? -1 : static_cast<int>(sel%4);
    }
    const unsigned *selection = uid == flavor_uid ? &bank : uid == HS_XSAT_ID ? &xsat_bank : uid == HS_BUS_ID ? &bus_bank
        : uid == HS_PUMP_ID ? &pump_bank : uid == HS_DISTRESSOR_ID ? &dist_bank : uid == HS_TUBE_STRIP_ID ? &tube_bank : uid == HS_HEAT_ID ? &heat_bank
        : uid == HS_CHANNEL_ID ? &channel_bank : nullptr;
    if (!selection) return -1;
    *count = static_cast<int>(uid == flavor_uid ? bank_count : uid == HS_XSAT_ID ? xsat_count : uid == HS_BUS_ID ? bus_count
        : uid == HS_PUMP_ID ? pump_count : uid == HS_DISTRESSOR_ID ? dist_count : uid == HS_TUBE_STRIP_ID ? tube_count : uid == HS_HEAT_ID ? heat_count : channel_count);
    unsigned at = __atomic_load_n(selection, __ATOMIC_RELAXED);
    return at < static_cast<unsigned>(*count) ? static_cast<int>(at) : -1;
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
    if (preset_open() && !question_active() && preset_cc(cc, value)) return 1;
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
    if (preset_open() && !question_active() && preset_note(note, value, shift)) return 1;
    if (note == 13 && uid == HS_EQ2_ID && !question_active()) {
        b2_paged(uid, value, shift);
        return 1;
    }
    if (note == 13 && (uid == flavor_uid || uid == HS_CHANNEL_ID || uid == HS_XSAT_ID || uid == HS_BUS_ID || uid == HS_PUMP_ID || uid == HS_HEAT_ID || uid == HS_DISTRESSOR_ID || uid == HS_TUBE_STRIP_ID) && !question_active()) {
        b2_paged(uid, value, shift);
        return 1;
    }
    return next_note ? next_note(note, value, shift) : 0;
}

extern "C" int fxmap_get_row(void *context, int id, void *output)
{
    unsigned uid = insert_open();
    if (!output || (uid != flavor_uid && uid != HS_CHANNEL_ID && uid != HS_XSAT_ID && uid != HS_BUS_ID && uid != HS_EQ2_ID && uid != HS_PUMP_ID && uid != HS_HEAT_ID && uid != HS_BASS_FOCUS_ID && uid != HS_DISTRESSOR_ID && uid != HS_TUBE_STRIP_ID) || id != -1)
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
        if (preset_open()) {
            snprintf(row->name, sizeof row->name, "HS Para EQ PRESETS");
            for (int &parameter : row->parameters) parameter = -1;
            snprintf(row->labels[0], sizeof row->labels[0], "Slot %u", __atomic_load_n(&preset_slot, __ATOMIC_RELAXED) + 1);
            snprintf(row->labels[1], sizeof row->labels[1], "#%u", __atomic_load_n(&preset_gen, __ATOMIC_RELAXED));
            return 1;
        }
        /* Band in the title: bands 5-24 share default settings, so without it their frames are
           pixel-identical and SampQOL's duplicate skip never redraws the band change. */
        snprintf(row->name,sizeof row->name,band ? "HS Para EQ BAND %02u" : "HS Para EQ GLOBAL",band);
        memcpy(row->labels,page.labels,sizeof row->labels);
        for (int i=0;i<6;++i)
            row->parameters[i]=band && page.parameters[i]>=0
                ? EQ_FIRST+(band-1)*EB_COUNT+page.parameters[i] : page.parameters[i];
        return 1;
    }
    memset(row, 0, sizeof *row);
    if (uid == HS_BASS_FOCUS_ID) {   /* one page, no presets: only here so the title is the name, not the LADSPA label */
        static constexpr char bass_labels[6][16] = {"Crossover", "Bass", "Mono Bass", "", "", ""};
        memcpy(row->name, "HS Bass Focus", sizeof "HS Bass Focus");
        for (int i = 0; i < 6; ++i) row->parameters[i] = i < 3 ? i : -1;
        memcpy(row->labels, bass_labels, sizeof row->labels);
        return 1;
    }
    if (uid == flavor_uid) memcpy(row->name, flavor_name, sizeof flavor_name);
    else if (uid == HS_XSAT_ID) memcpy(row->name, "HS Drive Sat", sizeof "HS Drive Sat");
    else if (uid == HS_BUS_ID) memcpy(row->name, "HS Bus Compressor", sizeof "HS Bus Compressor");
    else if (uid == HS_PUMP_ID) memcpy(row->name, "HS Pump", sizeof "HS Pump");
    else if (uid == HS_DISTRESSOR_ID) memcpy(row->name, "HS Crush", sizeof "HS Crush");
    else if (uid == HS_TUBE_STRIP_ID) memcpy(row->name, "HS Tube Strip", sizeof "HS Tube Strip");
    else if (uid == HS_HEAT_ID) memcpy(row->name, "HS Heat", sizeof "HS Heat");
    else memcpy(row->name, "HS Channel Strip", sizeof "HS Channel Strip");
    if (preset_open()) {
        for (int &parameter : row->parameters) parameter = -1;
        snprintf(row->labels[0], sizeof row->labels[0], "Slot %u", __atomic_load_n(&preset_slot, __ATOMIC_RELAXED) + 1);
        snprintf(row->labels[1], sizeof row->labels[1], "#%u", __atomic_load_n(&preset_gen, __ATOMIC_RELAXED));
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
