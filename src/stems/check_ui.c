// host check: cc -DSTEMS_HOST -o /tmp/check_ui check_ui.c hs_stems_ui.c stems_pad.c -lm && /tmp/check_ui [out.ppm]
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stems_ui.h"

static uint32_t *load(const char *path) {  /* a 320x240 P6 lab screenshot */
    static uint32_t px[8][320 * 240]; static int n;
    uint32_t *p = px[n++ % 8];
    FILE *f = fopen(path, "rb"); char hdr[32];
    assert(f && fgets(hdr, sizeof hdr, f));
    if (!strchr(hdr + 2, ' ')) { assert(fgets(hdr, sizeof hdr, f) && fgets(hdr, sizeof hdr, f)); }
    for (int i = 0; i < 320 * 240; i++) { unsigned char c[3]; assert(fread(c, 1, 3, f) == 3); p[i] = c[0] << 16 | c[1] << 8 | c[2]; }
    fclose(f);
    return p;
}
static void dump(const uint32_t *px, const char *path) {
    FILE *f = fopen(path, "wb"); fprintf(f, "P6 320 240 255\n");
    for (int i = 0; i < 320 * 240; i++) { unsigned char c[3] = {px[i] >> 16, px[i] >> 8, px[i]}; fwrite(c, 1, 3, f); }
    fclose(f);
}
static void pad(const char *ppm, int ok, int track, int idx) {
    int t = -1, p = -1, r = stems_pad_label(load(ppm), 320, &t, &p);
    if (r != (ok ? 0 : -1) || (ok && (t != track || p != idx))) { fprintf(stderr, "%s: r=%d t=%d p=%d\n", ppm, r, t, p); assert(0); }
}

int main(int argc, char **argv) {
    char d[256], b[64];
    assert(!stems_out("/media/c/Samples/Loops/Soul 92bpm.wav", d, sizeof d, b, sizeof b));
    assert(!strcmp(d, "/media/c/Samples/Loops/Soul 92bpm Stems") && !strcmp(b, "Soul 92bpm"));
    assert(stems_out("/x/.wav", d, sizeof d, b, sizeof b));                 /* no name */
    assert(stems_is_wav("/a/B.WAV") && !stems_is_wav("rel.wav") && stems_is_wav("/a/b.MP3") && !stems_is_wav("/a/b.flac") && !stems_is_wav(0));

    /* browser: stock, the same with SHIFT held, and the Sample screen */
    uint32_t *br = load("test/browser.ppm"), *brs = load("test/browser-shift.ppm"), *sam = load("test/sample-screen.ppm");
    assert(stems_is_browser(br, 320) && !stems_is_shift_browser(br, 320));
    assert(!stems_is_browser(brs, 320) && stems_is_shift_browser(brs, 320));
    assert(!stems_is_browser(sam, 320) && !stems_is_shift_browser(sam, 320) && !stems_row(sam, 320));
    assert(stems_row(br, 320) && stems_row(br, 320) == stems_row(brs, 320));  /* same row, SHIFT or not */

    /* the pad label, with SampQOL's "S03 T01" and without (stock) */
    pad("test/pad-mod-A06.ppm", 1, 1, 5);
    pad("test/pad-mod-E01.ppm", 1, 1, 64);
    pad("test/pad-mod-shift-A01.ppm", 1, 1, 0);
    pad("test/pad-stock-A10.ppm", 1, 0, 9);
    pad("test/pad-stock-shift-A01.ppm", 1, 0, 0);
    pad("test/sample-screen.ppm", 1, 0, 0);
    pad("test/browser.ppm", 0, 0, 0);
    pad("test/browser-shift.ppm", 0, 0, 0);
    pad("test/fx-page.ppm", 0, 0, 0);

    /* the autosave lookup */
    static const char xpj[] =
        "ACVS\n1.3.0.12\nSerialisableProjectData\njson\nLinux\n"
        "{\"data\": {\"version\": 28, \"tracks\": ["
        "{\"name\": \"Kit\", \"program\": {\"type\": 0, \"drum\": {\"instruments\": ["
        "  {\"layersv\": [{\"sampleName\": \"Soul\", \"sampleFile\": \"Soul Loop 92bpm001.wav\"}]},"
        "  {\"layersv\": [{\"sampleFile\": \"\"}, {\"sampleFile\": \"Caf\\u00e9 \\\"live\\\" [v2].wav\"}]},"
        "  {\"layersv\": [{\"sampleFile\": \"\"}]},"
        "  {\"layersv\": [{\"sampleFile\": \"../escape.wav\"}]}]}}},"
        "{\"name\": \"Submix 1\", \"program\": {\"type\": 8, \"x\": [1, {\"a\": \"}]\"}]}},"
        "{\"name\": \"Keys\", \"program\": {\"type\": 2, \"keygroup\": {}}},"
        "{\"name\": \"Kit 2\", \"program\": {\"type\": 0, \"drum\": {\"instruments\": ["
        "  {\"layersv\": [{\"sampleFile\": \"Kick.WAV\"}]}]}}}"
        "], \"currentTrackIndex\": 3}}";
    char f[128];
    assert(!stems_xpj_sample(xpj, 1, 0, f, sizeof f) && !strcmp(f, "Soul Loop 92bpm001.wav"));
    assert(!stems_xpj_sample(xpj, 1, 1, f, sizeof f) && !strcmp(f, "Caf\xc3\xa9 \"live\" [v2].wav"));
    assert(!strcmp(stems_xpj_sample(xpj, 1, 2, f, sizeof f), "No sample on this pad"));
    assert(!strcmp(stems_xpj_sample(xpj, 1, 3, f, sizeof f), "No sample on this pad"));   /* a path, not a name */
    assert(!strcmp(stems_xpj_sample(xpj, 1, 9, f, sizeof f), "Not a drum program"));     /* no such pad */
    assert(!strcmp(stems_xpj_sample(xpj, 2, 0, f, sizeof f), "Not a drum program"));     /* T02 = Keys (submix skipped) */
    assert(!stems_xpj_sample(xpj, 3, 0, f, sizeof f) && !strcmp(f, "Kick.WAV"));
    assert(!stems_xpj_sample(xpj, 0, 0, f, sizeof f) && !strcmp(f, "Kick.WAV"));         /* currentTrackIndex 3 */
    assert(!strcmp(stems_xpj_sample(xpj, 4, 0, f, sizeof f), "Track not found"));
    assert(!strcmp(stems_xpj_sample("{\"data\": {}}", 1, 0, f, sizeof f), "Can't read the project"));
    char small[8];
    assert(stems_xpj_sample(xpj, 1, 0, small, sizeof small));                           /* doesn't fit */

    /* stems onto pads: the free pads in the source pad's bank, from it on, then from the bank's start */
    static const char xpf[] =
        "{\"data\": {\"tracks\": [{\"program\": {\"type\": 0, \"drum\": {\"instruments\": ["
        "{\"layersv\": [{\"sampleFile\": \"Loop.wav\"}]},"                 /* A01: the source */
        "{\"layersv\": [{\"sampleFile\": \"\"}, {\"sampleFile\": \"\"}]},"  /* A02 free */
        "{\"layersv\": [{\"sampleFile\": \"\"}, {\"sampleFile\": \"Hat.wav\"}]},"  /* A03: layer 2 used */
        "{\"layersv\": [{\"sampleName\": \"x\"}]},"                        /* A04: no file = free */
        "{\"layersv\": [{\"sampleFile\": \"\"}]}, {\"layersv\": []},"       /* A05, A06 free */
        "{\"layersv\": [{\"sampleFile\": \"S.wav\"}]}, {\"layersv\": [{\"sampleFile\": \"S.wav\"}]},"
        "{\"layersv\": [{\"sampleFile\": \"S.wav\"}]}, {\"layersv\": [{\"sampleFile\": \"S.wav\"}]},"
        "{\"layersv\": [{\"sampleFile\": \"S.wav\"}]}, {\"layersv\": [{\"sampleFile\": \"S.wav\"}]},"
        "{\"layersv\": [{\"sampleFile\": \"S.wav\"}]}, {\"layersv\": [{\"sampleFile\": \"S.wav\"}]},"
        "{\"layersv\": [{\"sampleFile\": \"S.wav\"}]}, {\"layersv\": [{\"sampleFile\": \"\"}]},"  /* A16 free */
        "{\"layersv\": [{\"sampleFile\": \"\"}]}"                          /* B01 free, other bank */
        "]}}}], \"currentTrackIndex\": 0}}";
    int fp[8]; const char *why = NULL;
    assert(stems_free_pads(xpf, 1, 0, 0, fp, 4, &why) == 4 && fp[0] == 1 && fp[1] == 3 && fp[2] == 4 && fp[3] == 5);
    assert(stems_free_pads(xpf, 0, 0, 0, fp, 8, &why) == 5 && fp[4] == 15);          /* current track; A16 last */
    assert(stems_free_pads(xpf, 1, 15, 15, fp, 4, &why) == 4 && fp[0] == 1 && fp[3] == 5);  /* from A16: wraps */
    assert(stems_free_pads(xpf, 1, 3, -1, fp, 2, &why) == 2 && fp[0] == 3 && fp[1] == 4);   /* from an empty pad: it first */
    assert(stems_free_pads(xpf, 1, 16, 16, fp, 4, &why) == 0);                       /* B01 skipped, B02+ missing */
    assert(stems_free_pads(xpf, 2, 0, 0, fp, 4, &why) == -1 && !strcmp(why, "Track not found"));
    assert(stems_free_pads(xpj, 2, 0, 0, fp, 4, &why) == -1 && !strcmp(why, "Not a drum program"));
    char pn[40];
    assert(!stems_pads_note((int[]){1, 2, 3, 4}, 4, 4, pn, sizeof pn) && !strcmp(pn, "On pads A02-A05"));
    assert(!stems_pads_note((int[]){1, 3, 4, 5}, 4, 4, pn, sizeof pn) && !strcmp(pn, "On pads A02 A04 A05 A06"));
    assert(!stems_pads_note((int[]){1, 2}, 2, 2, pn, sizeof pn) && !strcmp(pn, "On pads A02 A03"));
    assert(!stems_pads_note((int[]){17}, 1, 1, pn, sizeof pn) && !strcmp(pn, "On pad B02"));
    assert(!stems_pads_note((int[]){14, 15}, 2, 4, pn, sizeof pn) && !strcmp(pn, "On A15 A16, bank full"));
    assert(!stems_pads_note(NULL, 0, 4, pn, sizeof pn) && !strcmp(pn, "Bank full: not on pads"));
    assert((int)strlen("On A14 A15 A16, bank full") * 8 <= 240);                    /* fits the box line */

    /* picks and the file names they make (as hs_stems.c names them) */
    char pk[8], n[64];
    assert(!stems_pick(15, 0, pk, sizeof pk) && !strcmp(pk, "VDBO"));
    assert(!stems_pick(12, 1, pk, sizeof pk) && !strcmp(pk, "BO+"));
    assert(stems_pick(0, 0, pk, sizeof pk));
    assert(!stems_out_name("Loop", 12, 1, 0, n, sizeof n) && !strcmp(n, "Loop-Bass+Other.wav"));
    assert(stems_out_name("Loop", 12, 1, 1, n, sizeof n));
    assert(!stems_out_name("Loop", 10, 0, 0, n, sizeof n) && !strcmp(n, "Loop-Drums.wav"));
    assert(!stems_out_name("Loop", 10, 0, 1, n, sizeof n) && !strcmp(n, "Loop-Other.wav"));
    assert(stems_out_name("Loop", 10, 0, 2, n, sizeof n));

    /* drawing, dumped for a look: argv[1] browser progress, argv[2] the chooser on the Sample screen */
    static uint32_t px[320 * 240];
    stems_paint_b1(px, 320, "Stems");
    stems_paint_box(px, 320, "Stems: Soul 92bpm", 42, "Separating... 42%  ~0:12 left");
    if (argc > 1) dump(px, argv[1]);
    uint32_t *ch = load("test/pad-mod-A06.ppm");
    stems_paint_tabs(ch, 320, "Cancel", "Sum", "Go");
    stems_paint_chooser(ch, 320, "Stems: pad A06", 12, 1, 1);
    if (argc > 2) dump(ch, argv[2]);
    uint32_t *sh = load("test/pad-mod-shift-A01.ppm");
    stems_paint_b3(sh, 320, "Stems");
    if (argc > 3) dump(sh, argv[3]);
    /* stems onto pads (argv[4] done, argv[5] bank full, argv[6] loading): the boxes over the Sample screen */
    const char *done[3][2] = {{"Vocals, Drums, Bass, Other", "On pads A02-A05"},
                              {"Vocals, Drums, Bass, Other", "On A15 A16, bank full"}, {NULL, NULL}};
    for (int i = 0; i < 3; ++i) {
        uint32_t *bx = load("test/pad-mod-A06.ppm");
        if (done[i][0]) { stems_paint_box(bx, 320, "Saved in Samples/Stems", -1, done[i][0]); stems_paint_note(bx, 320, done[i][1]); }
        else { stems_paint_tabs(bx, 320, "Cancel", "", ""); stems_paint_box(bx, 320, "Stems: Leroys C#Min 75bpm", 100, "Loading onto pads..."); }
        if (argc > 4 + i) dump(bx, argv[4 + i]);
    }
    puts("check_ui: ok");
    return 0;
}
