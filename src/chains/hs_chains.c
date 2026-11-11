// HS track chains: rename a track and, if it has no inserts yet, the matching chain loads onto it.
// Each /tmp/hs-chains/NAME.xfx matches track names starting with the word NAME (any case); the longest
// NAME wins, so VOX.xfx covers "Vox Lead" and VOXBG.xfx can take "VoxBG 2". Aliases cover "Vocals", "Drm". Runs the firmware's own "Load Insert Effect Strip"
// command, so one UNDO removes the rename and the chain together.
// Saving: rename a dialled-in track to NAME# and its inserts + settings are saved as NAME.xfx in
// $HS_CHAINS_USER (on the card, outside the pinned files). Saved chains win over shipped ones.
// Firmware 1.3.0.12 only: find_base checks the MPC app's GNU build ID and does nothing on any other build.
// Build: gcc -O2 -Wall -Wextra -Werror -fPIC -shared hs_chains.c -o hs-chains.so
// Research and addresses: research/track-chains.md
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

// link-time addresses (add the load base)
#define RENAME_PERFORM_SLOT (0x6e43220 + 16 + 3 * 8)  // SetTrackNameCommand vtable, slot 3
#define FACTORY_VPTR 0x6d95318                         // CommandFactory
#define LOAD_STRIP 0x195c2e0                           // CommandFactory::loadInsertEffectStrip
// Saving a program's strip: the firmware's saver (0x2993cc0) wraps these four in a progress popup whose
// context is empty during a rename (null deref), so call them directly.
#define STRIP_INIT 0x298d3a0   // (builder*)
#define STRIP_FILL 0x28cc150   // (Program*, builder*)
#define STRIP_WRITE 0x2993810  // (builder*, File*, plain, 0) -> bool; plain=0 gzips
#define SAVE_TMP "/tmp/hs-chains-save.json"
#define STRIP_FREE 0x28d3830   // (builder*)
#define STRING_CTOR 0xf535c0                           // juce::String(const char*)
#define STRING_DTOR 0xf0a290                           // juce::String::~String
#define WHOLE_PROGRAM 0x101                            // loader target: the program, not a pad
static const uintptr_t program_vptrs[] = { 0x6e3abc8, 0x6ec4c80, 0x6ec5600, 0x6ec5e60, 0x6ec6860, 0x6ec6f78,
                                           0x6ec7700, 0x6ec7ec0, 0x6ec8290, 0x6ec8660, 0x6ec8a30, 0x6ec8e00,
                                           0x6ec91d0, 0x6ec9d00, 0x6eca450, 0x6ecaeb8 };
#ifndef CHAIN_DIR
#define CHAIN_DIR "/tmp/hs-chains"
#endif
#define LOG_FILE "/tmp/hs-chains.log"

static uintptr_t base, factory;
static const char *user_dir;  // $HS_CHAINS_USER: saved chains, checked before CHAIN_DIR

static void lg(const char *fmt, uintptr_t a, uintptr_t b)
{
    FILE *f = fopen(LOG_FILE, "a");
    if (f) { fprintf(f, fmt, a, b); fputc('\n', f); fclose(f); }
}

// A pointer we may read: plausible user address, 8-aligned, and its page is mapped.
static int ok(uintptr_t p)
{
    return p > 0x10000 && p < 0xffffffffffffull && !(p & 7) && !msync((void *)(p & ~0xfffUL), 4096, MS_ASYNC);
}
static int ok_range(uintptr_t p, size_t n) { return ok(p) && ok((p + n - 8) & ~7UL); }

// ponytail: one heap scan for the single CommandFactory, cached and re-checked before each use.
// Costs one pass over [heap] on the first matching rename; swap for a pointer path if that ever shows.
static uintptr_t find_factory(void)
{
    if (factory && ok(factory) && *(uintptr_t *)factory == base + FACTORY_VPTR) return factory;
    factory = 0;
    FILE *m = fopen("/proc/self/maps", "r");
    char line[512];
    while (m && !factory && fgets(line, sizeof line, m)) {
        unsigned long a, b;
        char perm[5];
        if (!strstr(line, "[heap]") || sscanf(line, "%lx-%lx %4s", &a, &b, perm) != 3 || perm[1] != 'w') continue;
        for (uintptr_t *w = (uintptr_t *)a; w < (uintptr_t *)b; ++w)
            if (*w == base + FACTORY_VPTR) { factory = (uintptr_t)w; break; }
    }
    if (m) fclose(m);
    return factory;
}

// The renamed Track: in the TrackPool, the Track whose name String (+0x820) shares the new name's text.
static uintptr_t find_track(uintptr_t pool, uintptr_t name)
{
    if (!ok_range(pool, 64 * 8)) return 0;
    for (int i = 0; i < 64; ++i) {
        uintptr_t arr = ((uintptr_t *)pool)[i];
        if (!ok_range(arr, 64 * 8)) continue;
        for (int j = 0; j < 64; ++j) {
            uintptr_t t = ((uintptr_t *)arr)[j];
            if (ok_range(t, 0x828) && *(uintptr_t *)(t + 0x820) == name) return t;
        }
    }
    return 0;
}

static uintptr_t find_program(uintptr_t track)
{
    if (!ok_range(track, 0x1000)) return 0;
    for (int i = 0; i < 0x1000 / 8; ++i) {
        uintptr_t p = ((uintptr_t *)track)[i];
        if (!ok(p)) continue;
        for (unsigned k = 0; k < sizeof program_vptrs / sizeof *program_vptrs; ++k)
            if (*(uintptr_t *)p == base + program_vptrs[k]) return p;
    }
    return 0;
}

// Insert slots: program+0x78 holds the strip at +2320; the strip keeps a vector of slots at +160.
// Returns the number of used slots, or -1 if the layout doesn't look right.
static int used_slots(uintptr_t program)
{
    uintptr_t strip = *(uintptr_t *)(program + 0x78 + 2320);
    if (!ok_range(strip, 176)) return -1;
    uintptr_t *begin = ((uintptr_t **)strip)[160 / 8], *end = ((uintptr_t **)strip)[168 / 8];
    long n = end - begin;
    if (n < 1 || n > 8 || !ok_range((uintptr_t)begin, n * 8)) return -1;
    int used = 0;
    for (long i = 0; i < n; ++i) used += begin[i] != 0;  // an Empty slot is a null pointer
    return used;
}

// The track name starts with NAME as a whole word: "Bass 2", "Bass-1" and "Bass2" match BASS,
// "Bassline" doesn't.
static int word_prefix(const char *track_name, const char *stem, size_t n)
{
    if (strncasecmp(track_name, stem, n)) return 0;
    char c = track_name[n];
    return !c || c == ' ' || c == '-' || c == '_' || (c >= '0' && c <= '9');
}

// Longest NAME.xfx the track name starts with (as a word), in dir; a tie keeps the earlier dir's file.
static void scan_chains(const char *dir, const char *track_name, char *path, size_t size, size_t *best_len)
{
    DIR *d = dir ? opendir(dir) : 0;
    if (!d) return;
    size_t best = *best_len;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n <= 4 || e->d_name[0] == '.' || strcasecmp(e->d_name + n - 4, ".xfx")) continue;
        n -= 4;
        if (n > best && word_prefix(track_name, e->d_name, n)) {
            best = n;
            snprintf(path, size, "%s/%s", dir, e->d_name);
        }
    }
    closedir(d);
    *best_len = best;
}

// SampQOL's rename presets and its Abbreviate button give other words for the shipped chains.
static const char *const aliases[][2] = {
    {"VOCALS", "VOX"}, {"VOCAL", "VOX"}, {"DRM", "DRUMS"}, {"DRUM", "DRUMS"},
};

static int find_chain(const char *track_name, char *path, size_t size)
{
    size_t best = 0;
    scan_chains(user_dir, track_name, path, size, &best);
    scan_chains(CHAIN_DIR, track_name, path, size, &best);
    for (size_t i = 0; !best && i < sizeof aliases / sizeof *aliases; ++i)
        if (word_prefix(track_name, aliases[i][0], strlen(aliases[i][0]))) {
            scan_chains(user_dir, aliases[i][1], path, size, &best);
            scan_chains(CHAIN_DIR, aliases[i][1], path, size, &best);
        }
    return best > 0;
}

// Next JSON string value after "key": copies it (unescaped) to out; returns the position after it,
// or 0 if missing or too long for out (a cut-off value would save a broken chain).
static const char *json_str(const char *p, const char *end, const char *key, char *out, size_t size)
{
    const char *k = strstr(p, key);
    if (!k || k >= end || !(k = strchr(k + strlen(key), '"')) || k >= end) return 0;
    size_t n = 0;
    for (++k; *k && *k != '"'; ++k) {
        if (*k == '\\' && k[1]) ++k;
        if (n + 1 >= size) return 0;
        out[n++] = *k;
    }
    out[n] = 0;
    return *k ? k + 1 : 0;
}

static long json_num(const char *p, const char *end, const char *key)
{
    const char *k = strstr(p, key);
    return k && k < end ? strtol(k + strlen(key), 0, 10) : -1;
}

static void xml_put(FILE *f, const char *tag, const char *s)
{
    fprintf(f, "      <%s>", tag);
    for (; *s; ++s)
        if (*s == '&') fputs("&amp;", f); else if (*s == '<') fputs("&lt;", f); else if (*s == '>') fputs("&gt;", f); else fputc(*s, f);
    fprintf(f, "</%s>\n", tag);
}

// The firmware saves strips as JSON, and its JSON loader needs a progress screen we can't give it
// from a rename; its .xfx XML loader doesn't. So rewrite the saved JSON as XML (all fields kept).
static int json_to_xfx(const char *json_path, const char *xfx_path)
{
    static char js[1 << 18];
    FILE *f = fopen(json_path, "r");
    if (!f) return 0;
    size_t n = fread(js, 1, sizeof js - 1, f);
    fclose(f);
    if (n == sizeof js - 1) return 0;  // too big to be sure we read it all
    js[n] = 0;
    const char *p = strstr(js, "\"effects\"");
    if (!p || !(p = strchr(p, '['))) return 0;
    const char *stop = p;  // the matching ']' of "effects" (strings skipped)
    for (int depth = 0, in_str = 0; *stop; ++stop) {
        if (in_str) { if (*stop == '\\' && stop[1]) ++stop; else if (*stop == '"') in_str = 0; continue; }
        if (*stop == '"') in_str = 1;
        else if (*stop == '[') ++depth;
        else if (*stop == ']' && --depth == 0) break;
    }
    if (!*stop) return 0;
    n = stop - js;
    js[n] = 0;
    char tmp[340];
    snprintf(tmp, sizeof tmp, "%s.new", xfx_path);
    FILE *o = fopen(tmp, "w");
    if (!o) return 0;
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<MPCVObject>\n  <Version>\n    <File_Version>2.1</File_Version>\n"
          "    <Application>MPC-V</Application>\n    <Application_Version>0.0.0.495</Application_Version>\n"
          "    <Platform>Linux</Platform>\n  </Version>\n  <InsertEffectStrip>\n", o);
    int count = 0;
    while ((p = strstr(p, "\"index\":"))) {
        const char *next = strstr(p + 8, "\"index\":");
        const char *end = next ? next : js + n;
        long index = json_num(p, end, "\"index\":");
        static const char *const fields[][2] = { { "\"name\"", "Name" }, { "\"descriptiveName\"", "DescriptiveName" },
            { "\"pluginFormatName\"", "FormatName" }, { "\"category\"", "Category" },
            { "\"manufacturerName\"", "ManufacturerName" }, { "\"fileOrIdentifier\"", "FileOrID" } };
        static char v[6][256], state[1 << 16];
        int ok = index >= 0 && index < 8;
        for (int i = 0; ok && i < 6; ++i) ok = json_str(p, end, fields[i][0], v[i], sizeof v[i]) != 0;
        if (!ok || !json_str(p, end, "\"state\"", state, sizeof state)) { count = -1; break; }
        const char *en = strstr(p, "\"enable\":");
        int enable = 1;
        if (en && en < end) { for (en += 9; *en == ' '; ++en) {} enable = *en != 'f'; }
        fprintf(o, "    <Insert%ld>\n      <Is64Bits>1</Is64Bits>\n", index + 1);
        for (int i = 0; i < 6; ++i) {
            xml_put(o, fields[i][1], v[i]);
            if (i == 5) fprintf(o, "      <UID>%ld</UID>\n      <IsInstrument>0</IsInstrument>\n      <NumInput>%ld</NumInput>\n"
                                   "      <NumOutput>%ld</NumOutput>\n      <ProgramNumber>%ld</ProgramNumber>\n",
                                json_num(p, end, "\"uid\":"), json_num(p, end, "\"numInputChannels\":"),
                                json_num(p, end, "\"numOutputChannels\":"), json_num(p, end, "\"programNumber\":"));
        }
        xml_put(o, "Data", state);
        fprintf(o, "    </Insert%ld>\n    <InsertEnable%ld>%s</InsertEnable%ld>\n", index + 1, index + 1, enable ? "True" : "False", index + 1);
        ++count;
        if (!next) break;
        p = next;
    }
    fputs("  </InsertEffectStrip>\n</MPCVObject>\n", o);
    int good = count > 0 && !fflush(o) && !ferror(o) && !fsync(fileno(o));  // count -1: an insert couldn't be read
    good = !fclose(o) && good;
    if (good) good = !rename(tmp, xfx_path);
    if (!good) unlink(tmp);  // the old chain stays as it was
    return good ? count : 0;
}

// NAME# -> user_dir/NAME.xfx (upper case). Only letters, digits, space and '-' make it into a file name.
static int save_path(const char *track_name, char *path, size_t size)
{
    size_t n = strlen(track_name);
    if (!user_dir || n < 2 || track_name[n - 1] != '#') return 0;
    while (n > 1 && track_name[n - 2] == ' ') --n;
    char stem[64];
    size_t k = 0, i = 0;
    while (track_name[i] == ' ') ++i;
    for (; i + 1 < n && k + 1 < sizeof stem; ++i) {
        char c = track_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-')) return 0;
        stem[k++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }
    stem[k] = 0;
    return k > 0 && snprintf(path, size, "%s/%s.xfx", user_dir, stem) < (int)size;
}

__attribute__((visibility("hidden"), used)) void hs_chains_after_rename(uintptr_t *cmd)
{
    const char *name = (const char *)cmd[0x60 / 8];  // juce::String text of the new name
    char chain[320];
    if (!ok((uintptr_t)name & ~7UL)) return;
    int save = save_path(name, chain, sizeof chain);
    if (!save && !find_chain(name, chain, sizeof chain)) return;
    uintptr_t track = find_track(cmd[0x50 / 8], (uintptr_t)name);
    uintptr_t program = track ? find_program(track) : 0;
    uintptr_t f = program ? find_factory() : 0;
    if (!f) { lg("skip: track %#lx program %#lx", track, program); return; }
    int used = used_slots(program);
    void (*str_ctor)(void *, const char *) = (void *)(base + STRING_CTOR);
    void (*str_dtor)(void *) = (void *)(base + STRING_DTOR);
    uintptr_t project = ((uintptr_t *)f)[5];
    void *file = 0;

    if (save) {
        if (used <= 0) { lg("skip save: no inserts on the track", 0, 0); return; }
        mkdir(user_dir, 0755);
        void (*init)(void *) = (void *)(base + STRIP_INIT);
        void (*fill)(uintptr_t, void *) = (void *)(base + STRIP_FILL);
        unsigned char (*write)(void *, void *, int, int) = (void *)(base + STRIP_WRITE);
        void (*release)(void *) = (void *)(base + STRIP_FREE);
        _Alignas(16) unsigned char builder[512];  // the firmware uses 336 bytes on its stack
        init(builder);
        fill(program, builder);
        str_ctor(&file, SAVE_TMP);  // a juce::File is just its path String
        int saved = write(builder, &file, 1, 0) ? json_to_xfx(SAVE_TMP, chain) : 0;
        str_dtor(&file);
        release(builder);
        unlink(SAVE_TMP);
        FILE *f2 = fopen(LOG_FILE, "a");
        if (f2) { fprintf(f2, saved ? "saved %d inserts to %s\n" : "SAVE FAILED %d %s\n", saved, chain); fclose(f2); }
        return;
    }
    if (used != 0) { lg("skip: %ld inserts already on the track", (uintptr_t)(long)used, 0); return; }
    // Only the XML form: the firmware's JSON (gzip) chains need a progress screen and would crash here.
    // Also skip anything over 1 MB or cut short (a card pulled mid-save): it must end with </MPCVObject>.
    char head[6] = { 0 }, tail[32] = { 0 };
    FILE *cf = fopen(chain, "r");
    long size = -1;
    if (cf) {
        if (fread(head, 1, 5, cf) != 5) head[0] = 0;
        if (!fseek(cf, 0, SEEK_END) && (size = ftell(cf)) > 32 && size < (1 << 20) && !fseek(cf, -31, SEEK_END))
            tail[fread(tail, 1, 31, cf)] = 0;
        fclose(cf);
    }
    if (strcmp(head, "<?xml") || !strstr(tail, "</MPCVObject>")) { lg("skip: %s is not a complete XML chain (%ld bytes)", (uintptr_t)chain, (uintptr_t)size); return; }

    void (*load)(uintptr_t, void *, uintptr_t, uintptr_t, int) = (void *)(base + LOAD_STRIP);
    str_ctor(&file, chain);
    load(f, &file, project + 0x128, program, WHOLE_PROGRAM);
    str_dtor(&file);
    FILE *f2 = fopen(LOG_FILE, "a");
    if (f2) { fprintf(f2, "loaded %s on program %#lx\n", chain, (unsigned long)program); fclose(f2); }
}

// Post-hook trampoline. The original may return an object through x8, so x8 must reach it intact;
// a C wrapper is not safe here.
__attribute__((visibility("hidden"))) uintptr_t hs_chains_orig;
__asm__(".text\n.type hs_chains_hook,%function\nhs_chains_hook:\n"
        "stp x29, x30, [sp, #-32]!\nmov x29, sp\nstr x0, [sp, #16]\n"
        "adrp x16, hs_chains_orig\nldr x16, [x16, :lo12:hs_chains_orig]\nblr x16\n"
        "str x0, [sp, #24]\nldr x0, [sp, #16]\nbl hs_chains_after_rename\nldr x0, [sp, #24]\n"
        "ldp x29, x30, [sp], #32\nret\n");
void hs_chains_hook(void);

// The stock MPC app of firmware 1.3.0.12 (the same check as src/fx/hs_fx_picker.c).
static const unsigned char build_id[] = {
    0x1d, 0xac, 0x7f, 0x48, 0x1a, 0x0f, 0xe8, 0xd1, 0x28, 0xe4,
    0x35, 0xac, 0xe8, 0x8d, 0x7e, 0x6a, 0xec, 0xe0, 0xc9, 0xc9
};

static int find_base(struct dl_phdr_info *i, size_t s, void *d)
{
    (void)s; (void)d;
    if (*i->dlpi_name) return 0;
    for (unsigned k = 0; k < i->dlpi_phnum; ++k) {
        const ElfW(Phdr) *seg = &i->dlpi_phdr[k];
        if (seg->p_type != PT_NOTE) continue;
        const unsigned char *note = (const void *)(i->dlpi_addr + seg->p_vaddr);
        size_t left = seg->p_filesz;
        while (left >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) h;
            memcpy(&h, note, sizeof h);
            size_t name = (h.n_namesz + 3) & ~3UL, desc = (h.n_descsz + 3) & ~3UL, len = sizeof h + name + desc;
            if (len > left) break;
            if (h.n_type == NT_GNU_BUILD_ID && h.n_namesz == 4 && h.n_descsz == sizeof build_id &&
                !memcmp(note + sizeof h, "GNU", 4) && !memcmp(note + sizeof h + name, build_id, sizeof build_id)) {
                base = i->dlpi_addr;
                return 1;
            }
            note += len;
            left -= len;
        }
    }
    return 1;  // the app is always first: any other build leaves base at 0 and the hook off
}

__attribute__((constructor)) static void init(void)
{
    dl_iterate_phdr(find_base, 0);
    if (!base) return;
    user_dir = getenv("HS_CHAINS_USER");
    if (user_dir && !*user_dir) user_dir = 0;
    long pg = sysconf(_SC_PAGESIZE);
    uintptr_t *slot = (uintptr_t *)(base + RENAME_PERFORM_SLOT);
    void *page = (void *)((uintptr_t)slot & ~(uintptr_t)(pg - 1));
    if (mprotect(page, pg, PROT_READ | PROT_WRITE)) return;
    hs_chains_orig = *slot;
    *slot = (uintptr_t)hs_chains_hook;
    mprotect(page, pg, PROT_READ);
}
