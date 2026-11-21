#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hs_ducker_gain.h"
#include "../common/qol_map.h"

typedef int (*map_fn)(void *, int, void *);
typedef int (*note_fn)(int, int, int);

enum {
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
static uintptr_t qol_base;
static const struct qol_map *qol;     /* NULL on an unknown SampQOL build: the page stays stock */
static atomic_int gain_page;

static int state(uintptr_t offset)
{
    return __atomic_load_n((const int *)(qol_base + offset), __ATOMIC_ACQUIRE);
}

int hs_ducker_gain_page(void)
{
    if (!qol || !state(qol->padmix_open) || !state(qol->insert_editor) ||
        state(qol->insert_uid) != DUCKER_UID)
        return -1;
    return atomic_load_explicit(&gain_page, memory_order_relaxed);
}

__attribute__((constructor)) static void install_gain_ui(void)
{
    next_note = (note_fn)dlsym(RTLD_NEXT, "padmix_note");
    next_map = (map_fn)dlsym(RTLD_NEXT, "fxmap_get_row");
    question_active = (int (*)(void))dlsym(RTLD_NEXT, "context_question_active");
    /* Bound to the SHA-pinned SampQOL builds in qol_map.h, with nothing interposed in between. */
    uintptr_t base = 0;
    const struct qol_map *map = qol_map(&base);
    if (!map || !question_active || !qol_owns((const void *)next_map, base) ||
        !qol_owns((const void *)next_note, base))
        return;
    qol_base = base;
    qol = map;
}

int padmix_note(int note, int value, int shift)
{
    int page = hs_ducker_gain_page();
    if (note == SCREEN_2 && !shift && page >= 0 && !question_active()) {
        if (value > 0) {
            /* No redraw call: SampQOL re-reads the knob map (fxmap_get_row) 5 times a second and
               redraws the top bar and knob bar when the K3 label changes. */
            atomic_store_explicit(&gain_page, !page, memory_order_relaxed);
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
