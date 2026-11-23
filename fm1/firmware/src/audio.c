#include "dsb.h"

#define VOICES 8

/* The audio interrupt owns synthesis, transport and recording. The panel is
 * the only event producer. Aligned indices publish complete queue entries. */
typedef struct { uint8_t type, note, velocity; } Event;
static volatile Event events[64];
static volatile uint32_t write_at, read_at, panic;
typedef struct {
    uint32_t phase, increment, age;
    int32_t gain;
    int32_t partial[4], held[4];  /* Q20, relative to the fundamental */
    uint16_t decay, damper;
    uint8_t note, released, harmonic_count;
} Voice;
static Voice voices[VOICES];
typedef struct { uint32_t position; int32_t gain; } DrumVoice;
typedef struct { const int16_t *samples; uint32_t count, step; } DrumSample;
#include "drum_kit.h"
#define DRUM_SAMPLE(name, step) {drum_##name, sizeof drum_##name / sizeof drum_##name[0], step}
static const DrumSample kit[] = {
    DRUM_SAMPLE(kick, 1), DRUM_SAMPLE(snare, 2),
    DRUM_SAMPLE(hat, 2), DRUM_SAMPLE(cowbell, 1)
};
typedef struct {
    uint8_t note, velocity, decay;
    uint16_t attack[4], held[4];  /* Q12 */
} PickupProfile;
#include "tine_profile.h"
static uint8_t pedal;
static int32_t preamp_dc, capture_low, capture_filter;
static DrumVoice drums[DRUM_COUNT];
typedef struct { uint32_t index, end, step, age; uint16_t fraction; int32_t gain; } ChopVoice;
static ChopVoice chop;
static uint32_t trem_phase, capture_frames;
static int32_t clock_left;
volatile Slice slices[SLICE_COUNT];
volatile uint32_t sample_count, capture_active, playing, current_step;
volatile uint32_t keys_peak, warm_voicing;
volatile uint32_t chop_pattern[STEP_COUNT];
int16_t recording_pool[CAPACITY] POOL;

static int32_t limit(int32_t value) {
    return value < -32767 ? -32767 : value > 32767 ? 32767 : value;
}

void send_event(uint8_t type, uint8_t note, uint8_t velocity) {
    uint32_t slot = write_at;
    uint32_t next = (slot + 1) & 63;
    if (next == read_at) {
        if (type == EVENT_NOTE || type == EVENT_PEDAL || type == EVENT_PANIC) panic = 1;
        return;
    }
    events[slot].type = type;
    events[slot].note = note;
    events[slot].velocity = velocity;
    write_at = next;
}

static int32_t blend(int32_t lo, int32_t hi, uint32_t weight) {
    return lo + ((hi - lo) * (int32_t)weight >> 8);
}

static PickupProfile velocity_profile(uint32_t first, uint32_t end, uint8_t velocity) {
    const PickupProfile *lo = 0, *hi = 0;
    for (uint32_t i = first; i < end; ++i) {
        const PickupProfile *p = &profile[i];
        if (p->velocity <= velocity && (!lo || p->velocity > lo->velocity)) lo = p;
        if (p->velocity >= velocity && (!hi || p->velocity < hi->velocity)) hi = p;
    }
    if (!lo) return *hi;
    if (!hi || hi == lo) return *lo;
    uint32_t weight = (velocity - lo->velocity) * 256u / (hi->velocity - lo->velocity);
    PickupProfile result = *lo;
    result.decay = blend(lo->decay, hi->decay, weight);
    for (uint32_t h = 0; h < 4; ++h) {
        result.attack[h] = blend(lo->attack[h], hi->attack[h], weight);
        result.held[h] = blend(lo->held[h], hi->held[h], weight);
    }
    return result;
}

static void pickup_profile(Voice *voice, uint8_t note, uint8_t velocity) {
    const uint32_t count = sizeof profile / sizeof profile[0];
    uint32_t lo = 0, hi = 0, end = 0;
    while (end < count) {
        lo = hi;
        hi = end++;
        while (end < count && profile[end].note == profile[hi].note) ++end;
        if (profile[hi].note >= note) break;
    }
    PickupProfile lower = velocity_profile(lo, hi ? hi : end, velocity);
    PickupProfile upper = velocity_profile(hi, end, velocity);
    uint32_t weight = hi ? (uint32_t)((note > upper.note ? upper.note : note) - lower.note)
                      * 256u / (upper.note - lower.note) : 0;
    voice->decay = blend(lower.decay, upper.decay, weight);
    voice->damper = 390 + note * 3;
    voice->harmonic_count = 1;
    uint32_t sum = 4096;
    for (uint32_t h = 0; h < 4; ++h) {
        uint32_t attack = blend(lower.attack[h], upper.attack[h], weight);
        uint32_t held = blend(lower.held[h], upper.held[h], weight);
        if (voice->increment > 0x70000000u / (h + 2) || (attack < 12 && held < 12)) {
            attack = 0;
            held = 0;
        }
        if (attack || held) voice->harmonic_count = h + 2;
        voice->partial[h] = attack << 8;
        voice->held[h] = held << 8;
        sum += attack > held ? attack : held;
    }
    /* The bark uses more pickup headroom at hard velocities. */
    uint32_t velocity_gain = 9000 + velocity * velocity * 10;
    voice->gain = (int32_t)(velocity_gain * 6144u / (sum + 2048)) * 24;
}

static void note_event(uint8_t note, uint8_t velocity) {
    Voice *choice = &voices[0];
    for (uint32_t i = 0; i < VOICES; ++i) {
        Voice *v = &voices[i];
        if (v->gain && v->note == note) v->released = !velocity && pedal ? 2 : 1;
        if (!v->gain || v->gain < choice->gain) choice = v;
    }
    if (!velocity) return;
    choice->phase = 0;
    choice->increment = note_inc(note);
    choice->age = 0;
    pickup_profile(choice, note, velocity);
    choice->note = note;
    choice->released = 0;
}

static void hit(uint32_t drum, uint32_t velocity) {
    drums[drum].position = 0;
    drums[drum].gain = (int32_t)velocity * 192;
}

static void trigger_slice(uint32_t index, uint32_t velocity) {
    static const uint32_t increments[] = {
        16384,17358,18390,19484,20643,21870,23170,24548,26008,27554,29193,30929,
        32768,34716,36781,38968,41285,43740,46341,49097,52016,55109,58386,61858,65536
    };
    uint32_t start = slices[index].start, end = slices[index].end;
    int32_t pitch = slices[index].pitch;
    if (capture_active || start >= end || end > sample_count) return;
    chop.index = start;
    chop.end = end;
    chop.fraction = 0;
    chop.step = increments[pitch + 12];
    chop.age = 0;
    chop.gain = (int32_t)velocity * 256;
}

static void finish_capture(void) {
    uint32_t length = capture_frames >> 1;
    for (uint32_t i = 0; i < SLICE_COUNT; ++i) {
        slices[i].start = length * i / SLICE_COUNT;
        slices[i].end = length * (i + 1) / SLICE_COUNT;
        slices[i].pitch = 0;
    }
    sample_count = length;
    capture_active = 0;
}

static void process_events(void) {
    while (read_at != write_at) {
        Event e = events[read_at];
        read_at = (read_at + 1) & 63;
        switch (e.type) {
        case EVENT_NOTE: note_event(e.note, e.velocity); break;
        case EVENT_PEDAL:
            pedal = e.velocity >= 64;
            if (!pedal)
                for (uint32_t i = 0; i < VOICES; ++i)
                    if (voices[i].released == 2) voices[i].released = 1;
            break;
        case EVENT_TOGGLE_STEP: {
            uint16_t bit = (uint16_t)(1u << e.velocity);
            project.pattern[e.note] ^= bit;
            if (!playing && (project.pattern[e.note] & bit)) hit(e.note, 100);
            break;
        }
        case EVENT_RECORD_HIT:
            if (playing) project.pattern[e.note] |= (uint16_t)(1u << current_step);
            /* fall through */
        case EVENT_HIT: hit(e.note, e.velocity); break;
        case EVENT_SEQUENCE_SLICE:
            if (playing && sample_count && !capture_active)
                chop_pattern[current_step] = (uint32_t)e.note + 1;
            /* fall through */
        case EVENT_SLICE:
            trigger_slice(e.note, e.velocity);
            break;
        case EVENT_PLAY:
            playing = !playing;
            current_step = 15;
            clock_left = 0;
            break;
        case EVENT_CAPTURE:
            if (capture_active) finish_capture();
            else {
                capture_active = 1;
                sample_count = 0;
                capture_frames = 0;
                capture_low = capture_filter = 0;
                chop.end = 0;
            }
            break;
        case EVENT_PANIC: panic = 1; break;
        }
    }
    if (panic) {
        panic = 0;
        pedal = 0;
        for (uint32_t i = 0; i < VOICES; ++i) voices[i].released = 1;
    }
}

static int32_t piano(uint32_t tone) {
    int32_t mix = 0;
    for (uint32_t i = 0; i < VOICES; ++i) {
        Voice *v = &voices[i];
        if (v->gain <= 256) continue;
        v->phase += v->increment;
        int32_t fundamental = sine(v->phase);
        int32_t current = fundamental;
        int32_t upper = 0;
        if (v->harmonic_count > 1) {
            int32_t cosine = sine(v->phase + 0x40000000u);
            /* sin(n*x) from one sine/cosine pair. Intermediate products
             * use a 15-bit cosine and a bounded 16-bit sine recurrence. */
            int32_t previous = 0;
            for (uint32_t h = 0; h < v->harmonic_count - 1u; ++h) {
                int32_t next = ((current * cosine) >> 14) - previous;
                upper += ((next * (v->partial[h] >> 8)) >> 12) * (h & 1 ? -1 : 1);
                previous = current;
                current = next;
            }
        }
        int32_t sound = fundamental + (upper * (int32_t)(tone + 15) >> 6);
        int32_t gain = v->gain >> 8;
        if (v->age < 64) gain = gain * (int32_t)v->age >> 6;
        mix += (sound >> 2) * gain >> 15;
        if (!(++v->age & 15)) {
            uint32_t decay = v->released == 1 ? v->damper : v->decay;
            v->gain -= (int32_t)(((uint32_t)(v->gain >> 8) * decay) >> 8) + 1;
            for (uint32_t h = 0; h < v->harmonic_count - 1u; ++h)
                v->partial[h] -= (v->partial[h] - v->held[h]) >> 8;
        }
    }
    return mix;
}

static int32_t percussion(void) {
    int32_t mix = 0;
    for (uint32_t i = 0; i < DRUM_COUNT; ++i) {
        DrumVoice *voice = &drums[i];
        if (!voice->gain) continue;
        const DrumSample *sample = &kit[i];
        uint32_t index = voice->position >> 1;
        int32_t value = sample->samples[index];
        if ((voice->position & 1) && index + 1 < sample->count)
            value = (value + sample->samples[index + 1]) / 2;
        mix += value * voice->gain >> 15;
        voice->position += sample->step;
        if (voice->position >= sample->count * 2) voice->gain = 0;
    }
    return mix;
}

static int32_t sample_voice(void) {
    if (chop.index >= chop.end) return 0;
    uint32_t next = chop.index + 1;
    int32_t value = recording_pool[chop.index];
    if (next < chop.end)
        value += (recording_pool[next] - value) * (chop.fraction >> 1) / 32768;
    uint32_t fade = chop.age < 32 ? chop.age++ : 32;
    uint32_t remaining = chop.end - chop.index;
    if (remaining < fade) fade = remaining;
    value = value * (int32_t)fade / 32;
    uint32_t phase = chop.fraction + chop.step;
    chop.index += phase >> 16;
    chop.fraction = (uint16_t)phase;
    return value * chop.gain >> 15;
}

/* A low-cost voicing for amp-and-speaker color, in the shared piano path
 * ahead of resampling. The reference pickup spectrum remains the source. */
static int32_t warm_cabinet(int32_t input, uint32_t enabled) {
    static int32_t coupling, body, speaker_a, speaker_b, wet;
    int32_t target = enabled ? 256 : 0;
    if (wet < target) ++wet;
    if (wet > target) --wet;
    if (!wet) {
        coupling = body = speaker_a = speaker_b = 0;
        return input;
    }
    coupling += (input * 256 - coupling) / 128;
    int32_t coupled = input - coupling / 256;
    body += (coupled * 256 - body) / 16;
    int32_t colored = coupled + body / 1024;
    int32_t magnitude = colored < 0 ? -colored : colored;
    if (magnitude > 3900) magnitude = 3900 + (magnitude - 3900) * 5 / 8;
    colored = colored < 0 ? -magnitude : magnitude;
    speaker_a += (colored * 256 - speaker_a) * 3 / 8;
    speaker_b += (speaker_a - speaker_b) * 5 / 8;
    return input + ((speaker_b / 256 - input) * wet) / 256;
}

void render(int32_t *out, uint32_t frames) {
    process_events();
    uint32_t warm = warm_voicing;
    uint32_t tone = project.tone, drive = project.drive, depth = project.tremolo;
    uint32_t trem_step = 60000u + project.rate * 7300u;
    int32_t base = 661500 / project.bpm;
    int32_t offset = base * project.swing / 200;
    int32_t keys_level = project.level[KEYS] * 327 / 2;
    int32_t chops_level = project.level[CHOPS] * 327 / 2;
    int32_t drums_level = project.level[DRUMS] * 327 / 2;
    int32_t drive_gain = 4096 + (int32_t)drive * 4096 / 75;

    uint32_t peak = 0;
    for (uint32_t i = 0; i < frames; ++i) {
        if (playing && clock_left-- <= 0) {
            uint32_t step = (current_step + 1) & 15;
            current_step = step;
            clock_left = base + (step & 1 ? -offset : offset) - 1;
            for (uint32_t d = 0; d < DRUM_COUNT; ++d)
                if (project.pattern[d] & (1u << step)) hit(d, 108);
            uint32_t slot = chop_pattern[step];
            if (slot) trigger_slice(slot - 1, 118);
        }
        int32_t dry = piano(tone);
        int32_t driven = dry * drive_gain >> 12;
        /* Soft knee, with a lower positive threshold for a little asymmetry. */
        int32_t knee = driven < 0 ? 21200 : 18500;
        int32_t magnitude = driven < 0 ? -driven : driven;
        if (magnitude > knee) {
            int32_t over = magnitude - knee;
            int32_t room = 32767 - knee;
            magnitude = knee + (room * (over > 65535 ? 65535 : over)) /
                               (room + (over > 65535 ? 65535 : over));
        }
        dry = driven < 0 ? -magnitude : magnitude;
        preamp_dc += ((dry * 256) - preamp_dc) >> 10;
        if (!dry && preamp_dc > -1024 && preamp_dc < 1024) preamp_dc = 0;
        dry = warm_cabinet(limit(dry - (preamp_dc >> 8)), warm);
        if (capture_active) {
            /* Two low-pass stages ahead of 2:1 decimation. */
            capture_low += ((dry * 256) - capture_low) >> 1;
            capture_filter += (capture_low - capture_filter) >> 1;
            if (capture_frames & 1) recording_pool[capture_frames >> 1] = capture_filter >> 8;
            if (++capture_frames == CAPACITY * 2u) finish_capture();
        }
        uint32_t absolute = dry < 0 ? -dry : dry;
        if (absolute > peak) peak = absolute;
        trem_phase += trem_step;
        int32_t motion = (sine(trem_phase) * (int32_t)depth) >> 8;
        int32_t left = dry * (32768 - motion) >> 15;
        int32_t right = dry * (32768 + motion) >> 15;
        int32_t rhythm = (percussion() * drums_level >> 14) + (sample_voice() * chops_level >> 14);
        out[2 * i] += limit((left * keys_level >> 14) + rhythm);
        out[2 * i + 1] += limit((right * keys_level >> 14) + rhythm);
    }
    keys_peak = peak > keys_peak ? peak : keys_peak - (keys_peak >> 4);
}
