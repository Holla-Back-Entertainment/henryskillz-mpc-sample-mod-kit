#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"
#include "hs_eq2.h"
#include "hs_pump.h"
#include "hs_heat.h"
#include "hs_distressor.h"
#include "hs_bass_focus.h"

enum {
    HS_SATURATOR = 0x485301,
    HS_AIR_EXCITER = 0x485302,
    DYNAMICS = 2, EQ_FILTER = 3, HARMONIC = 4,
    NATIVE_UID = 0x48,
    DESCRIPTION_SIZE = 96,
    MAX_CATEGORIES = 64
};

static const char plugin_path[] = "/tmp/hs-ladspa-test/hs-pro-fx-ladspa.so";
static const char category_name[] = "HS Effects";

struct fx_row {
    uint32_t uid;
    int preset;
    int group;
    char name[24];
};

static const struct fx_row hs_rows[] = {
    {HS_SATURATOR, -1, HARMONIC, "HS Saturator"},
    {HS_AIR_EXCITER, -1, HARMONIC, "HS Air Exciter"},
    {HS_CHANNEL_ID, -1, HARMONIC, "HS Channel Strip"},
    {HS_XSAT_ID, -1, HARMONIC, "HS Drive Sat"},
    {HS_BUS_ID, -1, DYNAMICS, "HS Bus Compressor"},
    {HS_EQ2_ID, -1, EQ_FILTER, "HS Para EQ"},
    {HS_PUMP_ID, -1, DYNAMICS, "HS Pump"},
    {HS_HEAT_ID, -1, HARMONIC, "HS Heat"},
    {HS_DISTRESSOR_ID, -1, DYNAMICS, "HS Crush"},
    {HS_BASS_FOCUS_ID, -1, EQ_FILTER, "HS Bass Focus"}
};
enum { FX_COUNT = sizeof hs_rows / sizeof *hs_rows };

struct native_descriptions {
    unsigned char **items;
    int capacity;
    int count;
};

struct native_category {
    char *name;
    char *subcategory;
    unsigned char *items;
    int capacity;
    int count;
};

struct native_catalog {
    struct native_category *begin;
    struct native_category *end;
    struct native_category *limit;
};

_Static_assert(sizeof(struct fx_row) == 36, "SampQOL FX row");
_Static_assert(sizeof(struct native_category) == 32 &&
               offsetof(struct native_category, count) == 28, "MPC effect category");

static uintptr_t stock_base;
static int (*next_list)(void *, struct fx_row *, int);
static unsigned char *(*find_manager)(uintptr_t);
static void (*delete_object)(void *);
static unsigned char *manager;
static pthread_mutex_t catalog_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t align4(size_t size)
{
    return (size + 3) & ~(size_t)3;
}

static int locate_stock(struct dl_phdr_info *image, size_t size, void *data)
{
    (void)size;
    (void)data;
    if (image->dlpi_name && *image->dlpi_name) return 0;

    static const unsigned char build_id[] = {
        0x1d, 0xac, 0x7f, 0x48, 0x1a, 0x0f, 0xe8, 0xd1, 0x28, 0xe4,
        0x35, 0xac, 0xe8, 0x8d, 0x7e, 0x6a, 0xec, 0xe0, 0xc9, 0xc9
    };
    for (unsigned i = 0; i < image->dlpi_phnum; ++i) {
        const ElfW(Phdr) *segment = &image->dlpi_phdr[i];
        if (segment->p_type != PT_NOTE) continue;
        const unsigned char *note = (const void *)(image->dlpi_addr + segment->p_vaddr);
        size_t remaining = segment->p_filesz;
        while (remaining >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) header;
            memcpy(&header, note, sizeof header);
            size_t name = align4(header.n_namesz);
            size_t desc = align4(header.n_descsz);
            size_t length = sizeof header + name + desc;
            if (length > remaining) break;
            const unsigned char *payload = note + sizeof header;
            if (header.n_type == NT_GNU_BUILD_ID && header.n_namesz == 4 &&
                header.n_descsz == sizeof build_id && !memcmp(payload, "GNU", 4) &&
                !memcmp(payload + name, build_id, sizeof build_id)) {
                stock_base = image->dlpi_addr;
                return 1;
            }
            note += length;
            remaining -= length;
        }
    }
    return 1;
}

__attribute__((constructor)) static void install_fx_picker(void)
{
    next_list = dlsym(RTLD_NEXT, "fxlist_get");
    find_manager = dlsym(RTLD_NEXT, "context_locate_plugin_manager");
    delete_object = dlsym(RTLD_DEFAULT, "_ZdlPv");
    Dl_info info;
    if (!next_list || !find_manager || !delete_object || !dladdr(next_list, &info))
        return;
    uintptr_t qol_base = (uintptr_t)info.dli_fbase;
    if ((uintptr_t)next_list - qol_base != 0x6ca94 ||
        (uintptr_t)find_manager - qol_base != 0x5f284)
        return;
    dl_iterate_phdr(locate_stock, NULL);
}

static int publish_category(struct native_catalog *catalog, void *format)
{
    void (*make_string)(char **, const char *) = (void *)(stock_base + 0xf535c0);
    void (*drop_string)(char **) = (void *)(stock_base + 0xf0a290);
    void (*discover)(void *, struct native_descriptions *, char **) =
        (void *)(stock_base + 0xeb7340);
    void (*drop_description)(void *) = (void *)(stock_base + 0xecd540);

    char *path;
    struct native_descriptions found = {0};
    make_string(&path, plugin_path);
    discover(format, &found, &path);
    drop_string(&path);

    unsigned char *items = found.count == FX_COUNT ? malloc(FX_COUNT * DESCRIPTION_SIZE) : NULL;
    for (int i = 0; items && i < FX_COUNT; ++i) {
        unsigned char *description = found.items[i];
        uint32_t ordinal;
        memcpy(&ordinal, description + NATIVE_UID, sizeof ordinal);
        if (ordinal != (uint32_t)i || strcmp(*(char **)description, hs_rows[i].name)) {
            free(items);
            items = NULL;
        }
    }

    for (int i = 0; i < found.count; ++i) {
        unsigned char *description = found.items[i];
        if (items) {
            unsigned char *entry = items + i * DESCRIPTION_SIZE;
            memcpy(entry, description, DESCRIPTION_SIZE);
            memcpy(entry + NATIVE_UID, &hs_rows[i].uid, sizeof hs_rows[i].uid);
        } else {
            drop_description(description);
        }
        delete_object(description);
    }
    free(found.items);
    if (!items) return 0;

    /* The native catalog owns the JUCE strings and description array. Publish
     * a new last category so the stock indexes and their undo commands stay valid. */
    struct native_category *category = catalog->end;
    make_string(&category->name, category_name);
    make_string(&category->subcategory, "");
    category->items = items;
    category->capacity = FX_COUNT;
    category->count = FX_COUNT;
    __atomic_store_n(&catalog->end, category + 1, __ATOMIC_RELEASE);
    return 1;
}

static int ensure_category(void)
{
    if (!manager) manager = find_manager(stock_base);
    if (!manager || *(uintptr_t *)manager != stock_base + 0x6da1f90) return 0;

    unsigned char *groups = *(void **)(manager + 0x1d8);
    if (!groups || *(uintptr_t *)groups != stock_base + 0x6e77eb0) return 0;
    struct native_catalog *catalog = (void *)(groups + 0x648);
    if (!catalog->begin || catalog->end <= catalog->begin ||
        catalog->limit < catalog->end || catalog->limit - catalog->begin > MAX_CATEGORIES)
        return 0;
    for (struct native_category *category = catalog->begin; category < catalog->end; ++category)
        if (category->name && !strcmp(category->name, category_name))
            return category->count == FX_COUNT;

    if (catalog->end == catalog->limit || access(plugin_path, R_OK)) return 0;
    void **formats = *(void ***)(manager + 0x70);
    int format_count = *(int *)(manager + 0x7c);
    if (!formats || format_count < 2 || format_count > 16 ||
        !formats[1] || *(uintptr_t *)formats[1] != stock_base + 0x6d747f0)
        return 0;
    return publish_category(catalog, formats[1]);
}

int fxlist_get(void *context, struct fx_row *out, int capacity)
{
    if (!next_list) return 0;
    int count = next_list(context, out, capacity);
    if (!stock_base || !context || !out || count <= 0 || capacity - count < FX_COUNT)
        return count;

    pthread_mutex_lock(&catalog_lock);
    int ready = ensure_category();
    pthread_mutex_unlock(&catalog_lock);
    if (!ready) return count;

    memcpy(out + count, hs_rows, sizeof hs_rows);
    return count + FX_COUNT;
}
