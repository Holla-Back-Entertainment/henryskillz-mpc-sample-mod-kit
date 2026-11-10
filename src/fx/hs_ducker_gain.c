#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hs_ducker_gain.h"

typedef int (*map_fn)(void *, int, void *);
typedef int (*note_fn)(int, int, int);

enum {
    PADMIX_OPEN = 0x1632688,
    INSERT_EDITOR = 0x16328dc,
    INSERT_UID = 0x16328f4,
    DUCKER_UID = 0x41437654,
    DUCKER_MAP_ROW = -11,
    SCREEN_2 = 13,
    MAIN_K3 = 2,
    MAKE_UP_PARAMETER = 7
};

struct fx_row {
    char name[24];
    int parameters[6];
    char labels[6][16];
};

_Static_assert(offsetof(struct fx_row, parameters) == 24 &&
               offsetof(struct fx_row, labels) == 48, "SampQOL effect map ABI");

static note_fn next_note;
static map_fn next_map;
static int (*question_active)(void);
static void (*refresh)(void);
static uintptr_t qol_base;
static atomic_int gain_page;

static int state(uintptr_t offset)
{
    return __atomic_load_n((const int *)(qol_base + offset), __ATOMIC_ACQUIRE);
}

int hs_ducker_gain_page(void)
{
    if (!qol_base || !state(PADMIX_OPEN) || !state(INSERT_EDITOR) ||
        state(INSERT_UID) != DUCKER_UID)
        return -1;
    return atomic_load_explicit(&gain_page, memory_order_relaxed);
}

__attribute__((constructor)) static void install_gain_ui(void)
{
    next_note = (note_fn)dlsym(RTLD_NEXT, "padmix_note");
    next_map = (map_fn)dlsym(RTLD_NEXT, "fxmap_get_row");
    question_active = (int (*)(void))dlsym(RTLD_NEXT, "context_question_active");
    refresh = (void (*)(void))dlsym(RTLD_NEXT, "padmix_force");
    Dl_info info;
    if (!next_note || !next_map || !question_active || !refresh ||
        !dladdr((const void *)next_map, &info))
        return;
    uintptr_t base = (uintptr_t)info.dli_fbase;
    /* Bound to the SHA-pinned SampQOL 0.2.0 build in the lab package. */
    if ((uintptr_t)next_map - base != 0x6d31c ||
        (uintptr_t)next_note - base != 0xa3bc8 ||
        (uintptr_t)refresh - base != 0xa3300)
        return;
    qol_base = base;
}

int padmix_note(int note, int value, int shift)
{
    int page = hs_ducker_gain_page();
    if (note == SCREEN_2 && !shift && page >= 0 && !question_active()) {
        if (value > 0) {
            atomic_store_explicit(&gain_page, !page, memory_order_relaxed);
            refresh();
        }
        return 1;
    }
    return next_note ? next_note(note, value, shift) : 0;
}

int fxmap_get_row(void *context, int id, void *output)
{
    int result = next_map ? next_map(context, id, output) : 0;
    if (result && id == DUCKER_MAP_ROW && hs_ducker_gain_page() == 1) {
        struct fx_row *row = output;
        row->parameters[MAIN_K3] = MAKE_UP_PARAMETER;
        memcpy(row->labels[MAIN_K3], "Make-Up Gain", sizeof "Make-Up Gain");
    }
    return result;
}
