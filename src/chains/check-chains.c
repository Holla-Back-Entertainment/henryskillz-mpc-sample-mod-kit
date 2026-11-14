// Chain-name matching check (runs in Linux, e.g. the hs-flavor-tools image):
// gcc -O2 -o /tmp/cc check-chains.c chains_toast.c -ldl -lpthread && /tmp/cc
#define _GNU_SOURCE
#include <assert.h>
#define CHAIN_DIR "/tmp/hs-chains-check"
#include "hs_chains.c"

static const char *match(const char *name)
{
    static char path[320];
    if (!find_chain(name, path, sizeof path)) return "";
    return strrchr(path, '/') + 1;
}

int main(void)
{
    mkdir(CHAIN_DIR, 0755);
    const char *files[] = {"VOX.xfx", "VOXBG.xfx", "DRUMS.xfx", "BASS.xfx", "KICK.xfx", "BASSLINE.xfx"};
    for (int i = 0; i < 6; ++i) { char p[64]; snprintf(p, sizeof p, CHAIN_DIR "/%s", files[i]); fclose(fopen(p, "w")); }
    assert(!strcmp(match("Vox Lead"), "VOX.xfx"));
    assert(!strcmp(match("Vox Chop"), "VOX.xfx"));
    assert(!strcmp(match("VoxBG 2"), "VOXBG.xfx"));
    assert(!strcmp(match("Bass"), "BASS.xfx") && !strcmp(match("bass2"), "BASS.xfx") && !strcmp(match("Bass-1"), "BASS.xfx"));
    assert(!strcmp(match("Bassline 2"), "BASSLINE.xfx") && !strcmp(match("Kick"), "KICK.xfx"));
    assert(!*match("Kickdrum") && !*match("Voxel") && !*match("Drumstick"));
    assert(!strcmp(match("Vocals"), "VOX.xfx") && !strcmp(match("Vocal 2"), "VOX.xfx"));
    assert(!strcmp(match("Drm"), "DRUMS.xfx") && !strcmp(match("Drums"), "DRUMS.xfx") && !strcmp(match("Drum 1"), "DRUMS.xfx"));
    assert(!*match("Keys"));
    puts("check-chains: OK");
    return 0;
}
