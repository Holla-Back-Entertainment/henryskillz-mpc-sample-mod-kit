/* check-presets-migrate: Crush presets saved under the pre-rename folder move into Presets/hs_crush/ without
 * overwriting anything. Host check: cc -O2 -Wall -Wextra -Werror -I. check-presets-migrate.c -o /tmp/cpm && /tmp/cpm */
#include <assert.h>
#include <stdlib.h>
#include "hs_preset_migrate.h"

static char root[256];

static void put(const char *dir, const char *name, const char *text)
{
    char d[400], p[500];
    snprintf(d, sizeof d, "%s/%s", root, dir);
    hs_preset_mkdirs(d);
    snprintf(p, sizeof p, "%s/%s", d, name);
    FILE *f = fopen(p, "w");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}

static int has(const char *dir, const char *name, const char *text)   /* file exists; text (if any) matches */
{
    char p[500], buf[64] = "";
    snprintf(p, sizeof p, "%s/%s/%s", root, dir, name);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    if (!fgets(buf, sizeof buf, f)) buf[0] = 0;
    fclose(f);
    return !text || !strcmp(buf, text);
}

static int dir_exists(const char *dir) { char p[400]; snprintf(p, sizeof p, "%s/%s", root, dir); return hs_preset_exists(p); }

int main(void)
{
    char tmpl[] = "/tmp/hs-migrate-XXXXXX";
    assert(mkdtemp(tmpl));
    snprintf(root, sizeof root, "%s", tmpl);

    /* nothing there at all: fine */
    assert(hs_preset_migrate_crush(root) == 0 && !dir_exists(HS_CRUSH_DIR));

    /* migrate: slots 5-8 move, the old folder goes */
    put(HS_CRUSH_LEGACY_DIR, "5.txt", "five");  put(HS_CRUSH_LEGACY_DIR, "6.txt", "six");
    put(HS_CRUSH_LEGACY_DIR, "7.txt", "seven"); put(HS_CRUSH_LEGACY_DIR, "8.txt", "eight");
    assert(hs_preset_migrate_crush(root) == 0);
    assert(has(HS_CRUSH_DIR, "5.txt", "five") && has(HS_CRUSH_DIR, "8.txt", "eight"));
    assert(!dir_exists(HS_CRUSH_LEGACY_DIR));

    /* no-overwrite: an existing hs_crush slot wins and the old file is kept; a free slot still moves */
    put(HS_CRUSH_LEGACY_DIR, "5.txt", "old five"); put(HS_CRUSH_LEGACY_DIR, "1.txt", "one");
    assert(hs_preset_migrate_crush(root) == -1);              /* something was left behind */
    assert(has(HS_CRUSH_DIR, "5.txt", "five"));               /* untouched */
    assert(has(HS_CRUSH_LEGACY_DIR, "5.txt", "old five"));    /* not lost */
    assert(has(HS_CRUSH_DIR, "1.txt", "one") && !has(HS_CRUSH_LEGACY_DIR, "1.txt", NULL));
    assert(dir_exists(HS_CRUSH_LEGACY_DIR));

    /* empty old folder: removed, hs_crush untouched */
    char p[500];
    snprintf(p, sizeof p, "%s/%s/5.txt", root, HS_CRUSH_LEGACY_DIR);
    assert(!unlink(p));
    assert(hs_preset_migrate_crush(root) == 0 && !dir_exists(HS_CRUSH_LEGACY_DIR));
    assert(has(HS_CRUSH_DIR, "5.txt", "five"));

    /* a second run is a no-op */
    assert(hs_preset_migrate_crush(root) == 0);

    char cmd[300];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
    assert(system(cmd) == 0);
    puts("check-presets-migrate: OK");
    return 0;
}
