/* hs_preset_migrate.h: one-time move of HS Crush presets from their pre-rename folder to Presets/hs_crush/.
 * Plain C, header only, so presets.inc (C++) and check-presets-migrate.c share it. */
#ifndef HS_PRESET_MIGRATE_H
#define HS_PRESET_MIGRATE_H
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Crush's preset folder before the rename (the old name is a trademark). It exists only so cards saved by earlier
 * builds keep their presets. Internal path: never show it on screen, in docs or in a user-facing log line. */
#define HS_CRUSH_LEGACY_DIR "hs_distressor"
#define HS_CRUSH_DIR "hs_crush"

static inline int hs_preset_exists(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

static inline void hs_preset_mkdirs(char *dir)   /* mkdir -p; dir is edited in place and restored */
{
    for (char *p = dir + 1; *p; ++p)
        if (*p == '/') { *p = 0; mkdir(dir, 0755); *p = '/'; }
    mkdir(dir, 0755);
}

/* Moves each <n>.txt from <root>/<legacy> into <root>/hs_crush unless hs_crush already has that file (never
 * overwrites; a skipped file stays in the old folder, so nothing is lost). Removes the old folder once empty.
 * Returns 0 when there is nothing left to do (including no old folder), -1 when something failed or was left
 * behind, so the caller may try again later. */
static inline int hs_preset_migrate_crush(const char *root)
{
    char old_dir[512], new_dir[512], from[640], to[640];
    if (snprintf(old_dir, sizeof old_dir, "%s/%s", root, HS_CRUSH_LEGACY_DIR) >= (int)sizeof old_dir ||
        snprintf(new_dir, sizeof new_dir, "%s/%s", root, HS_CRUSH_DIR) >= (int)sizeof new_dir) return -1;
    DIR *d = opendir(old_dir);
    if (!d) return errno == ENOENT ? 0 : -1;
    int rc = 0, made = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".txt")) continue;   /* only preset files; leave anything else */
        if (snprintf(from, sizeof from, "%s/%s", old_dir, e->d_name) >= (int)sizeof from ||
            snprintf(to, sizeof to, "%s/%s", new_dir, e->d_name) >= (int)sizeof to) { rc = -1; continue; }
        if (hs_preset_exists(to)) { rc = -1; continue; }                /* the newer file wins; keep the old one */
        if (!made) { hs_preset_mkdirs(new_dir); made = 1; }
        if (rename(from, to)) {
            fprintf(stderr, "hs presets: could not move a Crush preset (%s)\n", strerror(errno));
            rc = -1;
        }
    }
    closedir(d);
    if (rmdir(old_dir) && errno != ENOTEMPTY && errno != EEXIST) rc = -1;   /* empty: gone; otherwise it stays */
    return rc;
}
#endif
