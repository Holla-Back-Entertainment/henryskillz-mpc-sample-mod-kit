#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <memory>
#include <sys/mman.h>
#include <unistd.h>

static int base_address(dl_phdr_info *info, size_t, void *dest)
{
    if (*info->dlpi_name) return 0;
    static constexpr unsigned char build_id[] = {
        0x1d, 0xac, 0x7f, 0x48, 0x1a, 0x0f, 0xe8, 0xd1, 0x28, 0xe4,
        0x35, 0xac, 0xe8, 0x8d, 0x7e, 0x6a, 0xec, 0xe0, 0xc9, 0xc9
    };
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const auto &segment = info->dlpi_phdr[i];
        if (segment.p_type != PT_NOTE) continue;
        auto *p = reinterpret_cast<const unsigned char *>(info->dlpi_addr + segment.p_vaddr);
        size_t available = segment.p_filesz;
        while (available >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) note;
            memcpy(&note, p, sizeof note);
            size_t name = (static_cast<size_t>(note.n_namesz) + 3) & ~size_t(3);
            size_t desc = (static_cast<size_t>(note.n_descsz) + 3) & ~size_t(3);
            size_t len = sizeof note + name + desc;
            if (len > available) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
                note.n_descsz == sizeof build_id &&
                !memcmp(p + sizeof note, "GNU", 4) &&
                !memcmp(p + sizeof note + name, build_id, sizeof build_id)) {
                *static_cast<uintptr_t *>(dest) = info->dlpi_addr;
                return 1;
            }
            p += len;
            available -= len;
        }
    }
    return 1;
}

template<class T>
static T &field(void *object, unsigned offset)
{
    return *reinterpret_cast<T *>(static_cast<char *>(object) + offset);
}

struct Category {
    const char *name;
    const char *subcategory;
    char *items;
    int capacity;
    int count;
};

#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"
#include "hs_eq2.h"
#include "insert.inc"
#include "banks.inc"
