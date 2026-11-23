#ifndef DSB_H
#define DSB_H
#include "fm1.h"

enum { KEYS, CHOPS, DRUMS };
enum { EVENT_NOTE, EVENT_PEDAL, EVENT_HIT, EVENT_RECORD_HIT, EVENT_TOGGLE_STEP, EVENT_SLICE, EVENT_SEQUENCE_SLICE, EVENT_PLAY, EVENT_CAPTURE, EVENT_PANIC };
enum { SLICE_COUNT = 8, DRUM_COUNT = 4, STEP_COUNT = 16, CAPACITY = 88200 };

typedef struct {
    uint32_t magic;
    uint8_t tone, drive, tremolo, rate, bpm, swing, chord;
    uint8_t level[3];
    uint16_t pattern[DRUM_COUNT];
} Project;

typedef struct {
    uint32_t start, end;
    int8_t pitch;
} Slice;

extern volatile Project project;
extern volatile Slice slices[SLICE_COUNT];
extern volatile uint32_t sample_count, capture_active, playing, current_step;
extern volatile uint32_t keys_peak, warm_voicing;
extern volatile uint32_t chop_pattern[STEP_COUNT];
extern int16_t recording_pool[CAPACITY];
void send_event(uint8_t type, uint8_t note, uint8_t velocity);
void render(int32_t *out, uint32_t frames);

#endif
