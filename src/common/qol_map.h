/* Where SampQOL keeps what the kit reads, per mpcqol.so build. The build is told apart by where
 * mpcqol.so exports overlay_update; an unknown build gets NULL and every hook stays off, untouched.
 * Each 0.3.0 value is matched to its 0.2.0 twin by code in docs/dev/SAMPQOL-0.3.0-MAP.md.
 * Needs _GNU_SOURCE before the first include (dladdr, dl_iterate_phdr, RTLD_NOLOAD). */
#ifndef HS_QOL_MAP_H
#define HS_QOL_MAP_H
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <string.h>

struct qol_map {
    uintptr_t overlay_update;       /* identity: offset of the exported overlay_update */
    /* pad mixer state, offsets into mpcqol.so */
    uintptr_t padmix_open, insert_editor, insert_uid, insert_row, set_pending, editor;
    /* channel/pad mixer: snapshot (count at +4, strips from +12), per-view selected and editing */
    uintptr_t mix_snapshot, mix_selected, mix_editing;
    int strip_bytes;
    /* overlay layer ids: 320x31 header, 320x160 list, 320x40 soft keys, mixer console */
    int header, list, softkeys, mixer;
    /* return sites of the pad mixer's overlay_update / overlay_update_rect calls */
    uintptr_t at_header, at_header_list, at_header_ducker;
    uintptr_t at_list_name, at_list_name_rack, at_list_body, at_list_selector, at_list_effect;
    uintptr_t at_soft_selector, at_soft_knobs;
    uintptr_t rect_list_effect, rect_list_name, rect_soft_knobs;
};

static const struct qol_map qol_maps[] = {
    {   /* SampQOL 0.2.0, mpcqol.so sha256 fc9285ac... */
        0x61be4,
        0x1632688, 0x16328dc, 0x16328f4, 0x1632908, 0x1632960, 0x16333a0,
        0x1957ddc, 0x1957948, 0x1957930, 116,
        26, 27, 28, 33,
        0xabc60, 0xac684, 0xad644,
        0xad9e8, 0xab89c, 0xac2f0, 0xad460, 0xada58,
        0xad4ac, 0xad698,
        0xaca6c, 0xabb98, 0xad93c,
    },
    {   /* SampQOL 0.3.0, mpcqol.so sha256 facea6b4... */
        0x6e220,
        0x16d8398, 0x16d8604, 0x16d861c, 0x16d862c, 0x16d8704, 0x16d9160,
        0x19ff710, 0x19ff248, 0x19ff230, 120,
        25, 26, 27, 32,
        0xc2ec4, 0xc4d98, 0xc4890,
        0xc4b98, 0xc28b0, 0xc3558, 0xc43d8, 0xc4c80,
        0xc4434, 0xc48e8,
        0xc39fc, 0xc2df0, 0xc4b88,
    },
};

static inline int qol_is_mpcqol(const char *path)
{
    const char *name = path ? strrchr(path, '/') : NULL;
    return path && !strcmp(name ? name + 1 : path, "mpcqol.so");
}

static inline int qol_find_image(struct dl_phdr_info *image, size_t size, void *found)
{
    (void)size;
    if (!qol_is_mpcqol(image->dlpi_name)) return 0;
    *(struct dl_phdr_info *)found = *image;
    return 1;
}

/* The map for the loaded mpcqol.so, or NULL. *base gets its load address. */
static inline const struct qol_map *qol_map(uintptr_t *base)
{
    static const struct qol_map *map;
    static uintptr_t at;
    static int done;
    if (!__atomic_load_n(&done, __ATOMIC_ACQUIRE)) {
        struct dl_phdr_info image;
        memset(&image, 0, sizeof image);
        void *lib = dl_iterate_phdr(qol_find_image, &image)
            ? dlopen(image.dlpi_name, RTLD_LAZY | RTLD_NOLOAD) : NULL;
        /* mpcqol.so's own overlay_update: the kit interposes the name, so RTLD_DEFAULT would find ours */
        void *ov = lib ? dlsym(lib, "overlay_update") : NULL;
        for (unsigned i = 0; ov && i < sizeof qol_maps / sizeof *qol_maps; ++i)
            if ((uintptr_t)ov - (uintptr_t)image.dlpi_addr == qol_maps[i].overlay_update) {
                at = (uintptr_t)image.dlpi_addr;
                map = &qol_maps[i];
            }
        if (lib) dlclose(lib);
        __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
    }
    if (base) *base = at;
    return map;
}

/* 1 when fn (an RTLD_NEXT lookup) lands in mpcqol.so loaded at base, not in another interposer. */
static inline int qol_owns(const void *fn, uintptr_t base)
{
    Dl_info info;
    return fn && base && dladdr(fn, &info) && (uintptr_t)info.dli_fbase == base;
}

#endif
