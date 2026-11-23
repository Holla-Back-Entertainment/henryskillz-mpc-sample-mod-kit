#include "dsb.h"
#include "dsb_logo.h"

#define PROJECT_MAGIC 0x44534231u
#define NAVY RGB(9, 16, 30)
#define PANEL RGB(20, 32, 52)
#define LINE RGB(44, 61, 79)
#define MUTED RGB(150, 171, 190)
#define CYAN RGB(57, 234, 240)
#define RED RGB(245, 53, 70)
#define YELLOW RGB(255, 219, 104)
#define METAL RGB(102, 125, 148)
#define SHADOW RGB(7, 12, 21)
#define WHITE_KEY RGB(206, 222, 228)
#define DARK_KEY RGB(29, 44, 62)

volatile Project project = {
    .magic = PROJECT_MAGIC, .tone = 49, .drive = 14, .tremolo = 25, .rate = 49,
    .bpm = 91, .swing = 22, .level = {88, 80, 74},
    .pattern = {0x0901, 0x1010, 0x5555, 0x4000}
};

static const char *const section_names[] = {"KEYS", "CHOPS", "DRUMS"};
static const char *const drum_names[] = {"KICK", "SNARE", "HAT", "BELL"};
static uint8_t section, drum, selected_slice, mixer, chop_write;
static uint32_t last_draw;
static int dirty = 1;
static const char *status_text;
static int8_t octave;
static uint8_t local_mode[KEY_COUNT], local_notes[KEY_COUNT][4], local_count[KEY_COUNT];
static uint8_t held_notes[128];
static uint16_t previous_pattern[DRUM_COUNT];
static uint32_t previous_count, previous_capture, previous_step = 255, previous_play;

static uint32_t clamp(int32_t v, uint32_t lo, uint32_t hi) {
    if (v < (int32_t)lo) return lo;
    return (uint32_t)v > hi ? hi : (uint32_t)v;
}

static void title(const char *label, int x, int y, uint16_t color) {
    text(x, y, label, color, FONT_SMALL);
}

static void value(int x, int y, uint32_t v, uint16_t color) {
    char number[12];
    format_int(number, (int32_t)v);
    title(number, x, y, color);
}

static void disc(int x, int y, int radius, uint16_t color) {
    for (int row = -radius; row <= radius; ++row) {
        int half = radius;
        while (half * half + row * row > radius * radius) --half;
        rect(x - half, y + row, 2 * half + 1, 1, color);
    }
}

static void rotary(int x, int y, const char *label, uint32_t position) {
    static const int8_t ticks[][2] = {{-12,12},{-16,6},{-17,-2},{-13,-10},{-7,-15},
        {0,-17},{7,-15},{13,-10},{17,-2},{16,6},{12,12}};
    rect(x - 25, y - 24, 50, 66, PANEL);
    for (uint32_t i = 0; i < sizeof ticks / sizeof ticks[0]; ++i)
        rect(x + ticks[i][0] - 1, y + ticks[i][1] - 1, 2, 2, i * 10 <= position ? CYAN : METAL);
    disc(x, y + 2, 12, SHADOW);
    disc(x, y - 1, 11, METAL);
    disc(x, y, 10, DARK_KEY);
    uint32_t marker = (position + 5) / 10;
    line(x, y, x + ticks[marker][0] / 2, y + ticks[marker][1] / 2, WHITE);
    title(label, x - text_width(label, FONT_SMALL) / 2, y + 20, WHITE);
}

static void signal_leds(void) {
    static const uint16_t thresholds[] = {
        100, 150, 225, 335, 500, 750, 1120, 1680,
        2500, 3750, 5600, 8400, 12600, 16600, 22000, 30000
    };
    uint32_t peak = keys_peak;
    for (uint32_t i = 0; i < 16; ++i) {
        uint16_t color = i < 12 ? CYAN : i < 15 ? YELLOW : RED;
        rect(34 + i * 11, 201, 8, 6, peak >= thresholds[i] ? color : LINE);
    }
}

static void draw_header(void) {
    rect(0, 0, 240, 31, RED);
    for (int y = 0; y < LOGO_H; ++y) {
        for (int x = 0; x < LOGO_W; ++x) {
            uint32_t a = logo_a[y][x];
            if (a)
                pixel(10 + x, 3 + y, RGB(245 + 10 * a / 255,
                                         53 + 202 * a / 255, 70 + 185 * a / 255));
        }
    }
    title(playing ? "PLAY" : "STOP", 145, 8, WHITE);
    value(190, 8, project.bpm, WHITE);
    for (int i = 0; i < 3; ++i) {
        int x = 8 + i * 76;
        rect(x, 38, 72, 23, section == i ? CYAN : PANEL);
        title(section_names[i], x + 8, 41, section == i ? NAVY : MUTED);
    }
}

static void draw_keys(void) {
    text(12, 67, "TINE PIANO", WHITE, FONT_BIG);
    title(project.chord ? "HS CHORD" : warm_voicing ? "WARM CAB" : "DIRECT", 12, 102, CYAN);
    for (int k = 0; k < 10; ++k) {
        rect(141 + k * 8, 103, 7, 16, WHITE_KEY);
        rect(141 + k * 8, 117, 7, 2, METAL);
    }
    static const uint8_t accidentals[] = {0, 1, 3, 4, 5, 7, 8};
    for (uint32_t i = 0; i < sizeof accidentals; ++i)
        rect(146 + accidentals[i] * 8, 103, 5, 10, DARK_KEY);
    rotary(34, 151, "TONE", project.tone);
    rotary(91, 151, "DRIVE", project.drive);
    rotary(148, 151, "TREM", project.tremolo);
    rotary(205, 151, "RATE", project.rate);
    signal_leds();
}

static void draw_chops(void) {
    uint32_t length = sample_count;
    title(capture_active ? "RECORDING PIANO..." : length ? "RESAMPLE  /  TINE PIANO" : "REC TO CAPTURE PIANO", 12, 69, capture_active ? RED : WHITE);
    rect(12, 91, 216, 60, PANEL);
    line(12, 120, 227, 120, LINE);
    if (length) {
        uint32_t start = slices[selected_slice].start, end = slices[selected_slice].end;
        uint32_t dx = start * 216 / length;
        uint32_t width = end * 216 / length - dx;
        if (width) rect(12 + dx, 93, width, 56, LINE);
        uint16_t peaks[216];
        uint32_t maximum = 1;
        for (uint32_t x = 0; x < 216; ++x) {
            uint32_t lo = x * length / 216, hi = (x + 1) * length / 216;
            int32_t peak = 0;
            for (uint32_t i = lo; i < hi; i += 12) {
                int32_t s = recording_pool[i];
                if (s < 0) s = -s;
                if (s > peak) peak = s;
            }
            peaks[x] = (uint16_t)peak;
            if ((uint32_t)peak > maximum) maximum = peak;
        }
        for (uint32_t x = 0; x < 216; ++x) {
            int32_t half = peaks[x] * 26u / maximum;
            if (half) line(12 + x, 120 - half, 12 + x, 120 + half, CYAN);
        }
    }
    for (int i = 0; i < SLICE_COUNT; ++i) {
        int x = 12 + i * 27;
        rect(x, 162, 24, 23, SHADOW);
        rect(x, 158, 24, 23, i == selected_slice ? CYAN : PANEL);
        rect(x + 2, 159, 20, 2, i == selected_slice ? WHITE : METAL);
        value(x + 8, 161, i + 1, i == selected_slice ? NAVY : MUTED);
    }
    title("START", 12, 188, MUTED);
    value(61, 188, slices[selected_slice].start / 22, WHITE);
    title("END", 106, 188, MUTED);
    value(139, 188, slices[selected_slice].end / 22, WHITE);
    char semitones[12];
    format_int(semitones, slices[selected_slice].pitch);
    title(semitones, 198, 188, CYAN);
    title("st", 214, 188, MUTED);
}

static void draw_drums(void) {
    title("STEP SEQUENCER", 12, 72, WHITE);
    title("SWING", 154, 72, MUTED);
    value(203, 72, project.swing, CYAN);
    for (int d = 0; d < DRUM_COUNT; ++d) {
        int y = 99 + 25 * d;
        title(drum_names[d], 8, y, d == drum ? CYAN : MUTED);
        for (int step = 0; step < STEP_COUNT; ++step) {
            int x = 52 + 11 * step;
            int on = project.pattern[d] & (1u << step);
            rect(x, y + 3, 9, 14, SHADOW);
            rect(x, y + 2, 8, 12, on ? CYAN : (step % 4 ? PANEL : LINE));
            rect(x + 1, y + 3, 6, 1, on ? WHITE : METAL);
            if (playing && current_step == (unsigned)step)
                rect(x, y + 17, 8, 2, YELLOW);
        }
    }
}

static void draw(void) {
    clear(NAVY);
    draw_header();
    if (mixer) {
        title("SECTION MIXER", 12, 72, WHITE);
        for (int i = 0; i < 3; ++i) {
            int x = 51 + i * 68;
            int y = 174 - project.level[i] * 67 / 100;
            rect(x - 29, 95, 58, 110, PANEL);
            rect(x - 2, 106, 4, 70, SHADOW);
            for (int tick = 0; tick <= 6; ++tick) {
                rect(x - 15, 107 + tick * 11, 8, 1, LINE);
                rect(x + 7, 107 + tick * 11, 8, 1, LINE);
            }
            rect(x - 13, y - 3, 27, 10, SHADOW);
            rect(x - 13, y - 5, 27, 9, METAL);
            rect(x - 10, y - 2, 21, 2, CYAN);
            title(section_names[i], x - text_width(section_names[i], FONT_SMALL) / 2, 184, WHITE);
        }
    } else if (section == KEYS) draw_keys();
    else if (section == CHOPS) draw_chops();
    else draw_drums();
    rect(0, 216, 240, 24, PANEL);
    const char *foot = status_text;
    if (!foot) {
        if (mixer) foot = "GLO  CLOSE MIXER";
        else if (section == KEYS) foot = "ENV VOICING   REC RESAMPLE";
        else if (section == CHOPS) foot = capture_active ? "PLAY KEYS   REC TO FINISH" :
            chop_write ? "EDIT  CHOP RECORD ON" : "EDIT  CHOP RECORD OFF";
        else foot = "16 KEYS: STEPS / HOLD REC";
    }
    title(foot, 9, 220, status_text ? YELLOW : MUTED);
}

static void piano_down(int k, int note) {
    static const uint8_t major7[] = {0, 4, 7, 11};
    static const uint8_t minor7[] = {0, 3, 7, 10};
    static const uint8_t dominant7[] = {0, 4, 7, 10};
    static const uint8_t qualities[] = {0, 1, 1, 0, 2, 1, 1};
    static const uint8_t degrees[] = {0, 2, 4, 5, 7, 9, 11};
    const uint8_t *intervals = major7;
    if (project.chord) {
        /* Seven diatonic-quality slots repeated across the physical keybed. */
        int chord_slot = k % 7;
        note = 53 + octave * 12 + degrees[chord_slot] + (k / 7) * 12;
        intervals = qualities[chord_slot] == 2 ? dominant7 :
                    qualities[chord_slot] == 1 ? minor7 : major7;
    }
    local_count[k] = project.chord ? 4 : 1;
    for (int i = 0; i < local_count[k]; ++i) {
        uint8_t n = clamp(note + intervals[i], 0, 127);
        local_notes[k][i] = n;
        if (held_notes[n]++ == 0) send_event(EVENT_NOTE, n, 96);
    }
}

static void panel_keys(int pad_mode) {
    for (int k = 0; k < KEY_COUNT; ++k) {
        if (key_pressed(k)) {
            local_mode[k] = section;
            if (section == KEYS || (section == CHOPS && capture_active)) {
                local_mode[k] = KEYS;
                piano_down(k, KEY_FIRST_NOTE + k + octave * 12);
            } else if (section == CHOPS && k < SLICE_COUNT) {
                selected_slice = k;
                send_event(chop_write ? EVENT_SEQUENCE_SLICE : EVENT_SLICE, k, 112);
                dirty = 1;
            } else if (section == DRUMS) {
                if (pad_mode && k < DRUM_COUNT) {
                    send_event(EVENT_RECORD_HIT, k, 110);
                } else if (!pad_mode && k < STEP_COUNT) {
                    send_event(EVENT_TOGGLE_STEP, drum, k);
                    dirty = 1;
                }
            }
        }
        if (key_released(k) && local_mode[k] == KEYS) {
            for (int i = 0; i < local_count[k]; ++i) {
                uint8_t n = local_notes[k][i];
                if (--held_notes[n] == 0) send_event(EVENT_NOTE, n, 0);
            }
            local_count[k] = 0;
        }
    }
}

static void midi_input(void) {
    uint8_t st, a, b;
    for (int i = 0; i < 32 && midi_read(&st, &a, &b); ++i) {
        uint8_t ch = st & 15, command = st & 240;
        if (command == 0x80 || command == 0x90) {
            uint8_t vel = command == 0x90 ? b : 0;
            if (ch == 1) {
                if (vel && a >= 36 && a < 36 + SLICE_COUNT) send_event(EVENT_SLICE, a - 36, vel);
            } else if (ch == 9) {
                if (!vel) continue;
                if (a == 36) send_event(EVENT_HIT, 0, vel);
                if (a == 38 || a == 40) send_event(EVENT_HIT, 1, vel);
                if (a == 42 || a == 44) send_event(EVENT_HIT, 2, vel);
                if (a == 46 || a == 49) send_event(EVENT_HIT, 3, vel);
            } else send_event(EVENT_NOTE, a, vel);
        } else if (command == 0xb0 && ch != 1 && ch != 9) {
            if (a == 64) send_event(EVENT_PEDAL, 0, b);
            if (a == 120 || a == 123) send_event(EVENT_PANIC, 0, 0);
        }
    }
}

static void edit(int32_t k1, int32_t k2, int32_t k3, int32_t k4) {
    if (mixer) {
        project.level[KEYS] = clamp(project.level[KEYS] + k1, 0, 100);
        project.level[CHOPS] = clamp(project.level[CHOPS] + k2, 0, 100);
        project.level[DRUMS] = clamp(project.level[DRUMS] + k3, 0, 100);
    } else if (section == KEYS) {
        project.tone = clamp(project.tone + k1, 0, 100);
        project.drive = clamp(project.drive + k2, 0, 100);
        project.tremolo = clamp(project.tremolo + k3, 0, 100);
        project.rate = clamp(project.rate + k4, 0, 100);
    } else if (section == CHOPS) {
        uint32_t length = sample_count;
        selected_slice = clamp(selected_slice + k1, 0, SLICE_COUNT - 1);
        volatile Slice *slot = &slices[selected_slice];
        if (length && slot->end) {
            slot->start = clamp((int32_t)slot->start + k2 * 110, 0, slot->end - 1);
            slot->end = clamp((int32_t)slot->end + k3 * 110, slot->start + 1, length);
            slot->pitch = (int8_t)clamp(slot->pitch + k4 + 12, 0, 24) - 12;
        }
    } else {
        drum = clamp(drum + k1, 0, DRUM_COUNT - 1);
        project.level[DRUMS] = clamp(project.level[DRUMS] + k2, 0, 100);
        project.swing = clamp(project.swing + k3, 0, 80);
        project.bpm = clamp(project.bpm + k4, 60, 160);
    }
}

void setup(void) {
    Project saved;
    if (settings_load(&saved, sizeof saved) == 0 && saved.magic == PROJECT_MAGIC &&
        saved.bpm >= 60 && saved.bpm <= 160 && saved.swing <= 80 &&
        saved.tone <= 100 && saved.drive <= 100 && saved.tremolo <= 100 &&
        saved.rate <= 100 && saved.chord <= 1 && saved.level[0] <= 100 &&
        saved.level[1] <= 100 && saved.level[2] <= 100) project = saved;
    draw();
}

void loop(void) {
    if (button_pressed(BTN_HOME) || button_pressed(BTN_FX) || button_pressed(BTN_SEQ)) {
        section = button_pressed(BTN_HOME) ? KEYS : button_pressed(BTN_FX) ? CHOPS : DRUMS;
        mixer = 0;
        status_text = 0;
        dirty = 1;
    }
    if (button_pressed(BTN_GLO)) { mixer ^= 1; status_text = 0; dirty = 1; }
    if (button_pressed(BTN_PLAY)) send_event(EVENT_PLAY, 0, 0);
    if (button_pressed(BTN_REC) && section != DRUMS) send_event(EVENT_CAPTURE, 0, 0);
    if (button_pressed(BTN_ENV) && section == KEYS) { warm_voicing ^= 1; dirty = 1; }
    if (button_pressed(BTN_ARP) && section == KEYS) { project.chord ^= 1; dirty = 1; }
    if (button_pressed(BTN_EDIT) && section == CHOPS) { chop_write ^= 1; dirty = 1; }
    if (button_pressed(BTN_SEL) && section == DRUMS) { drum = (drum + 1) % DRUM_COUNT; dirty = 1; }
    if (button_pressed(BTN_OCT_UP) && octave < 2) ++octave;
    if (button_pressed(BTN_OCT_DOWN) && octave > -3) --octave;
    if (button_pressed(BTN_SAVE)) {
        if (playing || capture_active) status_text = "STOP BEFORE SAVING";
        else {
            Project saved = project;
            status_text = settings_save(&saved, sizeof saved) == 0 ? "SETTINGS SAVED" : "SAVE FAILED";
        }
        dirty = 1;
    }
    int32_t k1 = encoder(ENC_KNOB1), k2 = encoder(ENC_KNOB2);
    int32_t k3 = encoder(ENC_KNOB3), k4 = encoder(ENC_KNOB4);
    if (k1 || k2 || k3 || k4) {
        status_text = 0;
        edit(k1, k2, k3, k4);
        dirty = 1;
    }
    panel_keys(button_held(BTN_REC));
    midi_input();
    if (sample_count != previous_count || capture_active != previous_capture ||
        playing != previous_play || (section == DRUMS && current_step != previous_step)) {
        previous_count = sample_count;
        previous_capture = capture_active;
        previous_step = current_step;
        previous_play = playing;
        dirty = 1;
    }
    if (section == DRUMS) {
        for (int i = 0; i < DRUM_COUNT; ++i) {
            uint16_t pattern = project.pattern[i];
            if (previous_pattern[i] != pattern) {
                previous_pattern[i] = pattern;
                dirty = 1;
            }
        }
    }
    led_button(BTN_ENV, section == KEYS && warm_voicing);
    led_button(BTN_HOME, section == KEYS);
    led_button(BTN_FX, section == CHOPS);
    led_button(BTN_SEQ, section == DRUMS);
    led_button(BTN_PLAY, playing != 0);
    led_button(BTN_REC, capture_active || (section == DRUMS && button_held(BTN_REC)));
    static uint32_t last_meter;
    uint32_t now = millis();
    if (dirty && now - last_draw >= 34) {
        dirty = 0;
        last_draw = now;
        draw();
        last_meter = now;
    } else if (!dirty && section == KEYS && !mixer && now - last_meter >= 50) {
        signal_leds();
        last_meter = now;
    }
}

void audio(int32_t *out, uint32_t frames) {
    render(out, frames);
}
