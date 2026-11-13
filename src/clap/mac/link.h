/* macOS has no <link.h>. hs_eq2_view.h only uses dl_iterate_phdr to find the MPC's effect library;
 * the CLAP faceplate links the effects in and sets those pointers itself, so the search finds nothing. */
#ifndef HS_MAC_LINK_H
#define HS_MAC_LINK_H
#include <stddef.h>
struct dl_phdr_info { const char *dlpi_name; };
static inline int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *data)
{
    (void)cb; (void)data;
    return 0;
}
#endif
