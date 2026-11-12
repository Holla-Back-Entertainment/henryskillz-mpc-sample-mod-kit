/* presets.inc: the PRESET page of every paged HS effect, opened by holding B2 (included by banks.inc).
 * Hold B2 to open it. K1 (or the DATA wheel) picks slot 1-8, SHIFT+B3 loads it, holding B3 saves the current
 * settings into it (a hold, so a stray press can't overwrite a slot). SHIFT+B3 deletes the insert everywhere else;
 * on this page it only loads. A file's first line may carry a name in parentheses, shown on the slot.
 *
 * Save: reads the open instance's control ports through hs-pro-fx-ladspa.so (hs_ladspa_controls).
 * Load: SampQOL's own padmix_set(slot, param, 0..1000), the path its knobs use, so UNDO, the project
 * file and the knob display all follow. It takes one parameter at a time, so a thread feeds it.
 * Files: <card>/MPC-Sample/Presets/<label>/<n>.txt, "index value" per line in real units.
 * ponytail: 0..1000 is SampQOL's resolution, so continuous values land within 0.1% of their range. */
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

enum { PRESET_SLOTS = 8, PRESET_MAX = 256 };   /* Para EQ has 245 controls */
static unsigned preset_slot;                 /* 0..7 */
static char preset_msg[24];
static time_t preset_msg_at;
static int preset_busy;
static unsigned preset_gen;                  /* bumps on every message: changes the knob row so SampQOL repaints */
static char preset_names[8][15];            /* 14 chars fit a slot box */
static unsigned b3_token, b3_gen;

#include "factory.inc"

/* The starter preset built in for this effect's slot (0-7), or null. A saved file always wins. */
static const Factory *factory_find(const char *label, unsigned slot)
{
    for (const Factory &f : factory)
        if (f.slot == static_cast<int>(slot) && !strcmp(f.label, label)) return &f;
    return nullptr;
}

using controls_fn = int (*)(const void *, const char **, float *, float *, float *, unsigned char *, int);
using set_fn = void (*)(int, int, int);

static int find_hs_ladspa(dl_phdr_info *info, size_t, void *out)
{
    if (!strstr(info->dlpi_name, "hs-pro-fx-ladspa")) return 0;
    void *lib = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);   /* the firmware's private copy, pinned */
    if (lib) *static_cast<void **>(out) = lib;
    return lib != nullptr;
}

static controls_fn ladspa_controls()
{
    static controls_fn fn;
    if (!fn) {
        void *lib = nullptr;
        dl_iterate_phdr(find_hs_ladspa, &lib);
        if (lib) fn = reinterpret_cast<controls_fn>(dlsym(lib, "hs_ladspa_controls"));
    }
    return fn;
}

/* SampQOL 0.2.0: insert editor at qol+0x16333a0, open insert at +0xd0, its LADSPA handle at +0x130
   (same read as hs_eq2_view.h; hs_ladspa_controls rejects anything that is not a live HS instance). */
static const void *open_handle()
{
    if (!qol_base) return nullptr;
    auto *editor = *reinterpret_cast<char *const *>(qol_base + 0x16333a0);
    auto *insert = editor ? *reinterpret_cast<char *const *>(editor + 0xd0) : nullptr;
    return insert ? *reinterpret_cast<void *const *>(insert + 0x130) : nullptr;
}

static bool has_presets(unsigned uid) { return uid != flavor_uid; }
static unsigned eq_preset;                   /* Para EQ has no page word: 1 while its PRESETS page is up */

/* B2: a tap steps the effect's own pages (SHIFT steps back); holding it B2_HOLD_US opens PRESETS.
   On PRESETS a tap or a hold returns to the page it came from. PRESETS is "selection == count". */
enum { B2_HOLD_US = 500000 };
static unsigned b2_token, b2_gen, preset_return;

static void preset_toggle(unsigned uid)
{
    if (uid == HS_EQ2_ID) {
        __atomic_xor_fetch(&eq_preset, 1u, __ATOMIC_RELAXED);
        if (refresh) refresh();
        return;
    }
    unsigned count, *s = paged(uid, &count);
    if (!s || !has_presets(uid)) return;
    unsigned at = __atomic_load_n(s, __ATOMIC_RELAXED);
    if (at >= count) __atomic_store_n(s, preset_return < count ? preset_return : 0, __ATOMIC_RELAXED);
    else { preset_return = at; __atomic_store_n(s, count, __ATOMIC_RELAXED); }
    if (refresh) refresh();
}

static void *b2_hold(void *arg)
{
    unsigned token = static_cast<unsigned>(reinterpret_cast<uintptr_t>(arg));
    usleep(B2_HOLD_US);
    unsigned expect = token;                 /* still held and not released since: this press is a hold */
    if (__atomic_compare_exchange_n(&b2_token, &expect, token & ~1u, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        preset_toggle(insert_open());
    return nullptr;
}

/* B2 for a paged HS editor. value > 0 = press, 0 = release. The Para EQ pages inside eq_selection
   (band * 4 + page: 2 pages on GLOBAL, 4 on a band) and steps on release like the rest, so a hold can
   open PRESETS without also turning the page. */
static void b2_paged(unsigned uid, int value, int shift)
{
    unsigned count, *s = paged(uid, &count);
    bool eq = uid == HS_EQ2_ID;
    if (!s && !eq) return;
    if (value > 0) {
        unsigned token = (++b2_gen << 1) | 1;
        __atomic_store_n(&b2_token, token, __ATOMIC_RELEASE);
        pthread_t t;
        if (has_presets(uid) && !pthread_create(&t, nullptr, b2_hold, reinterpret_cast<void *>(static_cast<uintptr_t>(token)))) pthread_detach(t);
        return;
    }
    if (!(__atomic_exchange_n(&b2_token, 0u, __ATOMIC_ACQ_REL) & 1)) return;   /* a hold already acted */
    if (eq) {
        if (__atomic_load_n(&eq_preset, __ATOMIC_RELAXED)) { preset_toggle(uid); return; }
        unsigned sel = __atomic_load_n(&eq_selection, __ATOMIC_RELAXED), pages = sel/4 ? 4 : 2;
        __atomic_store_n(&eq_selection, sel/4*4 + (sel%4 + (shift ? pages - 1 : 1))%pages, __ATOMIC_RELAXED);
        if (refresh) refresh();
        return;
    }
    unsigned at = __atomic_load_n(s, __ATOMIC_RELAXED);
    if (at >= count) { preset_toggle(uid); return; }
    __atomic_store_n(s, (at + (shift ? count - 1 : 1)) % count, __ATOMIC_RELAXED);
    if (refresh) refresh();
}

static bool preset_open()
{
    unsigned uid = insert_open(), count;
    if (uid == HS_EQ2_ID) return __atomic_load_n(&eq_preset, __ATOMIC_RELAXED) != 0;
    unsigned *s = paged(uid, &count);
    return s && has_presets(uid) && __atomic_load_n(s, __ATOMIC_RELAXED) >= count;
}

static void preset_dir(char *out, size_t size, const char *label)
{
    const char *root = getenv("HS_PRESETS_DIR");
    snprintf(out, size, "%s/%s", root && *root ? root : "/tmp/hs-presets", label);
}

static void say(const char *fmt, unsigned n)
{
    snprintf(preset_msg, sizeof preset_msg, fmt, n);
    preset_msg_at = time(nullptr);
    __atomic_add_fetch(&preset_gen, 1, __ATOMIC_RELAXED);
}

static void preset_save(unsigned slot)
{
    auto controls = ladspa_controls();
    const char *label = nullptr;
    float val[PRESET_MAX], lo[PRESET_MAX], hi[PRESET_MAX];
    unsigned char integer[PRESET_MAX];
    int n = controls ? controls(open_handle(), &label, val, lo, hi, integer, PRESET_MAX) : -1;
    if (n <= 0 || !label) { say("CAN'T READ", 0); return; }
    char dir[512], path[600], pending[608];
    preset_dir(dir, sizeof dir, label);
    for (char *p = dir + 1; *p; ++p) if (*p == '/') { *p = 0; mkdir(dir, 0755); *p = '/'; }
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/%u.txt", dir, slot + 1);
    snprintf(pending, sizeof pending, "%s.new", path);
    FILE *f = fopen(pending, "w");
    if (!f) { say("SAVE FAILED", 0); return; }
    fprintf(f, "# %s preset: parameter index, value\n", label);
    for (int i = 0; i < n; ++i) fprintf(f, "%d %.9g\n", i, static_cast<double>(val[i]));
    bool ok = fflush(f) == 0;
    if (ok) ok = fsync(fileno(f)) == 0;
    if (fclose(f)) ok = false;
    if (ok) ok = rename(pending, path) == 0;
    if (!ok) unlink(pending);
    say(ok ? "SAVED %u" : "SAVE FAILED", slot + 1);
}

struct load_job { int slot; int n; int index[PRESET_MAX]; int value[PRESET_MAX]; };

static void *load_thread(void *arg)
{
    std::unique_ptr<load_job> job(static_cast<load_job *>(arg));
    static set_fn set = reinterpret_cast<set_fn>(dlsym(RTLD_DEFAULT, "padmix_set"));
    const int *pending = reinterpret_cast<const int *>(qol_base + 0x1632960);   /* padmix_set's one-at-a-time flag */
    for (int i = 0; set && i < job->n; ++i) {
        for (int w = 0; w < 200 && __atomic_load_n(pending, __ATOMIC_ACQUIRE); ++w) usleep(2000);
        set(job->slot, job->index[i], job->value[i]);
    }
    for (int w = 0; w < 200 && __atomic_load_n(pending, __ATOMIC_ACQUIRE); ++w) usleep(2000);
    if (refresh) refresh();
    __atomic_store_n(&preset_busy, 0, __ATOMIC_RELEASE);
    return nullptr;
}

static void preset_load(unsigned slot)
{
    auto controls = ladspa_controls();
    const char *label = nullptr;
    float val[PRESET_MAX], lo[PRESET_MAX], hi[PRESET_MAX];
    unsigned char integer[PRESET_MAX];
    int n = controls ? controls(open_handle(), &label, val, lo, hi, integer, PRESET_MAX) : -1;
    if (n <= 0 || !label) { say("CAN'T READ", 0); return; }
    char dir[512], path[600], line[96];
    preset_dir(dir, sizeof dir, label);
    snprintf(path, sizeof path, "%s/%u.txt", dir, slot + 1);
    FILE *f = fopen(path, "r");
    if (!f)
        if (const Factory *starter = factory_find(label, slot))
            f = fmemopen(const_cast<char *>(starter->text), strlen(starter->text), "r");
    if (!f) { say("SLOT %u EMPTY", slot + 1); return; }
    auto job = std::make_unique<load_job>();
    job->slot = state(0x1632908) + 1;                         /* SampQOL's selected insert row = the open slot */
    int i; float v;
    while (fgets(line, sizeof line, f) && job->n < PRESET_MAX)
        if (sscanf(line, "%d %f", &i, &v) == 2 && i >= 0 && i < n && std::isfinite(v)) {
            bool lg = (integer[i] & 2) && v > 0;                /* log ports: the host's knob is log(v/lo)/log(hi/lo) */
            float t = hi[i] <= lo[i] ? 0 : lg ? std::log(v / lo[i]) / std::log(hi[i] / lo[i]) : (v - lo[i]) / (hi[i] - lo[i]);
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            /* Already there: skip it. The Para EQ has 245 controls and a preset moves a handful. */
            if (std::fabs(v - val[i]) <= 1e-4f * (hi[i] - lo[i])) continue;
            /* integer ports: round up so the host's truncation lands on the saved step */
            job->index[job->n] = i;
            job->value[job->n++] = (integer[i] & 1) ? static_cast<int>(std::ceil(t * 1000 - 1e-3f)) : static_cast<int>(std::lround(t * 1000));
        }
    fclose(f);
    if (job->slot < 1 || job->slot > 4) { say("LOAD FAILED", 0); return; }
    if (!job->n) { say("LOADED %u", slot + 1); return; }      /* every control already matches */
    if (__atomic_exchange_n(&preset_busy, 1, __ATOMIC_ACQ_REL)) return;
    pthread_t t;
    load_job *raw = job.release();
    if (pthread_create(&t, nullptr, load_thread, raw)) { delete raw; preset_busy = 0; say("LOAD FAILED", 0); return; }
    pthread_detach(t);
    say("LOADED %u", slot + 1);
}

extern "C" const char *hs_preset_name(int i)       /* the slot's name, or null: hs-fx shows SAVED */
{
    return i >= 0 && i < 8 && preset_names[i][0] ? preset_names[i] : nullptr;
}

/* For hs-fx's panel: 1 while the PRESET page is up. used[i] = slot i+1 has a file. */
extern "C" int hs_preset_view(int *slot, unsigned char used[PRESET_SLOTS], const char **message)
{
    if (!preset_open()) return 0;
    *slot = static_cast<int>(__atomic_load_n(&preset_slot, __ATOMIC_RELAXED));
    auto controls = ladspa_controls();
    const char *label = nullptr;
    float val[PRESET_MAX], lo[PRESET_MAX], hi[PRESET_MAX];
    unsigned char integer[PRESET_MAX];
    int n = controls ? controls(open_handle(), &label, val, lo, hi, integer, PRESET_MAX) : -1;
    char dir[512], path[600];
    struct stat st;
    for (int i = 0; i < PRESET_SLOTS; ++i) {
        used[i] = 0;
        if (n <= 0 || !label) continue;
        preset_dir(dir, sizeof dir, label);
        snprintf(path, sizeof path, "%s/%d.txt", dir, i + 1);
        bool file = stat(path, &st) == 0;
        const Factory *starter = file ? nullptr : factory_find(label, static_cast<unsigned>(i));
        used[i] = file || starter;
        preset_names[i][0] = 0;
        if (starter) snprintf(preset_names[i], sizeof preset_names[i], "%s", starter->name);
        FILE *f = file ? fopen(path, "r") : nullptr;        /* ponytail: re-read per draw; 8 tiny files, draws are rare */
        char line[96];
        if (f && fgets(line, sizeof line, f)) {
            const char *a = strchr(line, '('), *b = a ? strchr(a, ')') : nullptr;
            if (a && b) snprintf(preset_names[i], sizeof preset_names[i], "%.*s", static_cast<int>(b - a - 1), a + 1);
        }
        if (f) fclose(f);
    }
    *message = time(nullptr) - preset_msg_at <= 2 ? preset_msg : "";
    return 1;
}

/* padmix_cc / padmix_note while the PRESET page is up: 1 = handled. */
static int preset_cc(int cc, int value)
{
    unsigned s = __atomic_load_n(&preset_slot, __ATOMIC_RELAXED);
    if (cc == 16) s = static_cast<unsigned>(value) * PRESET_SLOTS / 128;
    else if (cc == 100) { int d = value >= 64 ? value - 128 : value; int t = static_cast<int>(s) + d; s = t < 0 ? 0 : t > PRESET_SLOTS - 1 ? PRESET_SLOTS - 1 : static_cast<unsigned>(t); }
    else return cc == 17 || cc == 18;                        /* K2/K3 have nothing to move here */
    __atomic_store_n(&preset_slot, s, __ATOMIC_RELAXED);
    if (refresh) refresh();
    return 1;
}

enum { B3_HOLD_US = 800000 };

static void *b3_hold(void *arg)
{
    unsigned token = static_cast<unsigned>(reinterpret_cast<uintptr_t>(arg));
    usleep(B3_HOLD_US);
    unsigned expect = token;
    if (preset_open() && __atomic_compare_exchange_n(&b3_token, &expect, token & ~1u, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        preset_save(__atomic_load_n(&preset_slot, __ATOMIC_RELAXED));
        if (refresh) refresh();
    }
    return nullptr;
}

/* SHIFT+B3 loads; B3 held B3_HOLD_US saves; a short B3 only says how. */
static int preset_note(int note, int value, int shift)
{
    if (note != 14) return 0;
    if (value > 0 && shift) { preset_load(__atomic_load_n(&preset_slot, __ATOMIC_RELAXED)); if (refresh) refresh(); return 1; }
    if (value > 0) {
        unsigned token = (++b3_gen << 1) | 1;
        __atomic_store_n(&b3_token, token, __ATOMIC_RELEASE);
        pthread_t t;
        if (!pthread_create(&t, nullptr, b3_hold, reinterpret_cast<void *>(static_cast<uintptr_t>(token)))) pthread_detach(t);
        return 1;
    }
    if (__atomic_exchange_n(&b3_token, 0u, __ATOMIC_ACQ_REL) & 1) { say("HOLD TO SAVE", 0); if (refresh) refresh(); }
    return 1;
}
