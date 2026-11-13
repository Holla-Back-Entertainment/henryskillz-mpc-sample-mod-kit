struct FlavorBank {
    const char *title;
    int module;
    int parameters[6];
    char labels[6][16];
    char units[6][4];
};

static constexpr FlavorBank banks[] = {
    {"GLOBAL", -1, {6, 10, 7, 8, 9, -1}, {"Intensity", "Width", "Gain", "High-Pass", "Low-Pass", ""}, {}},
    {"PITCH", 0, {0, 11, 12, 13, 16, 14}, {"Power", "Depth", "Vary", "Fast Rate", "Slow Rate", "Drift"}, {}},
    {"PITCH X-MOD", 0, {17, 18, 19, 15, 20, -1}, {"X-Mod", "Type", "Mix", "Stereo", "Fast/Slow", ""}, {}},
    {"DISTORTION", 1, {1, 21, 23, 22, 24, 27}, {"Power", "Depth", "Model", "Vary", "Mix", "Smooth"}, {}},
    {"DIST INPUT", 1, {25, 26, -1, -1, -1, -1}, {"Input HP", "Input LP", "", "", "", ""}, {}},
    {"DIGITAL", 2, {2, 30, 31, 28, 29, 36}, {"Power", "Bits", "Rate", "Depth", "Vary", "Smooth"}, {}},
    {"DIGITAL SHAPE", 2, {32, 33, 37, 34, 35, 38}, {"Glitch", "Length", "Envelope", "Input HP", "Input LP", "Stereo"}, {}},
    {"VINYL", 3, {3, 39, 41, 40, 42, 44}, {"Power", "Depth", "Crackle", "Vary", "Clicks", "Pops"}, {}},
    {"VINYL NOISE", 3, {43, 45, 46, 47, 48, 49}, {"Rate", "Rumble", "Noise", "Tone", "Stereo", "Hum"}, {}},
    {"VINYL ENVELOPE", 3, {50, 51, -1, -1, -1, -1}, {"Envelope", "Release", "", "", "", ""}, {}},
    {"VOLUME", 4, {4, 52, 60, 53, 54, 55}, {"Power", "Depth", "Dropouts", "Vary", "Fast Rate", "Slow Rate"}, {}},
    {"VOLUME SHAPE", 4, {57, 58, 59, 56, -1, -1}, {"Fast Drift", "Slow Drift", "Stereo", "Fast/Slow", "", ""}, {}},
    {"TIMBRE", 5, {5, 61, 63, 62, -1, -1}, {"Power", "Depth", "Profile", "Vary", "", ""}, {}}
};
/* Labels use stock wording: plain words, no units. The unit (last field) is drawn after the value
   by hs-fx (hs_knob_unit), as stock shows "-6.0 dB" under "Threshold". Knobs whose value hs-fx
   draws itself (ratios, switches, times with units) have no unit here. */
static constexpr FlavorBank channel_banks[] = {
    {"INPUT / OUTPUT", 0, {CH_INPUT, CH_DRIVE, CH_OUTPUT, -1, -1, -1},
        {"Input", "Drive", "Output", "", "", ""}, {"DB", "%", "DB"}},
    {"242 BLACK FILTERS", 1, {CH_HP, CH_LP, CH_EQ_FIRST, -1, -1, -1},
        {"High-Pass", "Low-Pass", "EQ First", "", "", ""}, {"HZ", "HZ"}},
    {"LOW EQ", 2, {CH_LF_DB, CH_LF_HZ, CH_LF_BELL, -1, -1, -1},
        {"Gain", "Frequency", "Bell", "", "", ""}, {"DB", "HZ"}},
    {"LOW MID EQ", 3, {CH_LM_DB, CH_LM_HZ, CH_LM_Q, -1, -1, -1},
        {"Gain", "Frequency", "Q", "", "", ""}, {"DB", "HZ"}},
    {"HIGH MID EQ", 4, {CH_HM_DB, CH_HM_HZ, CH_HM_Q, -1, -1, -1},
        {"Gain", "Frequency", "Q", "", "", ""}, {"DB", "HZ"}},
    {"HIGH EQ", 5, {CH_HF_DB, CH_HF_HZ, CH_HF_BELL, -1, -1, -1},
        {"Gain", "Frequency", "Bell", "", "", ""}, {"DB", "HZ"}},
    {"COMPRESSOR", 6, {CH_COMP_THRESHOLD, CH_COMP_RATIO, CH_MAKEUP,
                        CH_COMP_FAST, CH_COMP_RELEASE, CH_COMP_HARD},
        {"Threshold", "Ratio", "Make-Up", "Fast Attack", "Release", "Hard Knee"}, {"DB", "", "DB", "", "MS"}},
    {"GATE / EXPAND", 7, {CH_GATE_THRESHOLD, CH_GATE_RANGE, CH_GATE_RELEASE,
                         CH_GATE_FAST, CH_GATE_EXPAND, -1},
        {"Threshold", "Range", "Release", "Fast Attack", "Expand", ""}, {"DB", "DB", "MS"}},
    {"DE-ESSER", 8, {CH_DS_FREQ, CH_DS_THRESHOLD, CH_DS_RANGE,
                     CH_DS_MODE, -1, -1},
        {"Frequency", "Threshold", "Range", "Mode", "", ""}, {"HZ", "DB", "DB"}},
    HS_MOD_BANKS(CH_MOD, 0)
};
/* No Bypass knob on any HS page: SampQOL's B3 is the one Bypass (the old ports are ignored). */
static constexpr FlavorBank xsat_banks[] = {
    {"DRIVE", 0, {XS_DRIVE, XS_SHAPE, XS_DEPTH, -1, -1, -1},
        {"Drive", "Shape", "Depth", "", "", ""}, {"DB", "", "%"}},
    {"HARMONICS", 1, {XS_HARMONICS, XS_MIX, XS_HEADROOM, -1, -1, -1},
        {"Harmonics", "Mix", "Headroom +6", "", "", ""}, {"%", "%"}},
    {"INPUT / OUTPUT", 2, {XS_INPUT, XS_OUTPUT, -1, -1, -1, -1},
        {"Input", "Output", "", "", "", ""}, {"DB", "DB"}},
    HS_MOD_BANKS(XS_MOD, 0)
};
static constexpr FlavorBank bus_banks[] = {
    {"COMPRESSION", 0, {BUS_THRESHOLD, BUS_RATIO, BUS_MAKEUP, -1, -1, -1},
        {"Threshold", "Ratio", "Make-Up", "", "", ""}, {"DB", "", "DB"}},
    {"TIMING", 1, {BUS_ATTACK, BUS_RELEASE, BUS_HPF, -1, -1, -1},
        {"Attack", "Release", "Sidechain HP", "", "", ""}, {"", "", "HZ"}},
    {"PARALLEL", 2, {BUS_MIX, -1, -1, -1, -1, -1},
        {"Mix", "", "", "", "", ""}, {"%"}},
    HS_MOD_BANKS(BUS_MOD, 0)
};
static constexpr FlavorBank pump_banks[] = {
    {"COMPRESS", 0, {PU_INPUT, PU_RATIO, PU_OUTPUT, -1, -1, -1},
        {"Input", "Ratio", "Output", "", "", ""}, {"DB", "", "DB"}},
    {"TIMING", 1, {PU_ATTACK, PU_RELEASE, PU_ATMOD, -1, -1, -1},
        {"Attack", "Release", "Attack Mod", "", "", ""}, {}},
    {"COLOUR", 2, {PU_SAT, PU_DET_HP, PU_MIX, -1, -1, -1},
        {"Saturation", "Detector HP", "Mix", "", "", ""}, {"", "", "%"}},
    {"KEY", 3, {PU_KEY, PU_SEND, -1, -1, -1, -1},
        {"Key In", "Send", "", "", "", ""}, {}},
    HS_MOD_BANKS(PU_MOD, 0)
};
static constexpr FlavorBank dist_banks[] = {
    {"COMPRESS", 0, {DS_INPUT, DS_RATIO, DS_OUTPUT, -1, -1, -1}, {"Input", "Ratio", "Output", "", "", ""}, {}},
    {"TIMING", 1, {DS_ATTACK, DS_RELEASE, DS_DETECTOR, -1, -1, -1}, {"Attack", "Release", "Detector", "", "", ""}, {}},
    {"AUDIO", 2, {DS_AUDIO, DS_BRIT, DS_MIX, -1, -1, -1}, {"Audio", "British", "Mix", "", "", ""}, {"", "", "%"}},
    {"KEY", 3, {DS_KEY, DS_SEND, -1, -1, -1, -1}, {"Key In", "Send", "", "", "", ""}, {}},
    HS_MOD_BANKS(DS_MOD, 0)
};
static constexpr FlavorBank tube_banks[] = {
    {"TUBE", 0, {TS_DRIVE, TS_HPF, TS_OUTPUT, -1, -1, -1}, {"Drive", "High-Pass", "Output", "", "", ""}, {}},
    {"OPTO COMP", 1, {TS_THRESHOLD, TS_RATIO, TS_ATTACK, -1, -1, -1}, {"Threshold", "Ratio", "Attack", "", "", ""}, {}},
    {"COMP TIME", 2, {TS_RELEASE, TS_ORDER, TS_SC, -1, -1, -1}, {"Release", "EQ First", "Sidechain EQ", "", "", ""}, {}},
    {"BASS", 3, {TS_BASS_HZ, TS_BASS_DB, -1, -1, -1, -1}, {"Frequency", "Gain", "", "", "", ""}, {}},
    {"LOW MID", 4, {TS_LMID_HZ, TS_LMID_DB, TS_Q, -1, -1, -1}, {"Frequency", "Gain", "High Q", "", "", ""}, {}},
    {"HIGH MID", 5, {TS_HMID_HZ, TS_HMID_DB, TS_Q, -1, -1, -1}, {"Frequency", "Gain", "High Q", "", "", ""}, {}},
    {"TREBLE", 6, {TS_TREBLE_HZ, TS_TREBLE_DB, -1, -1, -1, -1}, {"Frequency", "Gain", "", "", "", ""}, {}},
    HS_MOD_BANKS(TS_MOD, 0)
};
static constexpr FlavorBank heat_banks[] = {
    {"CHARACTER", 0, {HE_CIRCUIT, HE_DRIVE, HE_WET, -1, -1, -1}, {"Circuit", "Drive", "Wet", "", "", ""}, {"", "", "DB"}},
    {"FILTER", 1, {HE_FTYPE, HE_FREQ, HE_RESO, -1, -1, -1}, {"Filter Type", "Frequency", "Resonance", "", "", ""}, {}},
    {"DIRT + EQ", 2, {HE_DIRT, HE_EQ_LO, HE_EQ_HI, -1, -1, -1}, {"Dirt", "EQ Low", "EQ High", "", "", ""}, {}},
    {"ENVELOPE", 3, {HE_ENV_DEPTH, HE_ENV_ATK, HE_ENV_REL, -1, -1, -1}, {"Env Depth", "Attack", "Release", "", "", ""}, {}},
    {"LFO", 4, {HE_LFO_DEPTH, HE_LFO_SPEED, HE_LFO_WAVE, -1, -1, -1}, {"LFO Depth", "Speed", "Wave", "", "", ""}, {}},
    {"OUTPUT", 5, {HE_FREQ_PAN, HE_MIX, HE_VOLUME, -1, -1, -1}, {"Freq Pan", "Mix", "Volume", "", "", ""}, {"", "%", "DB"}},
    HS_MOD_BANKS(HE_MOD, 0)
};
static constexpr FlavorBank eq_global[] = {
    {"GLOBAL",0,{EQ_INPUT,EQ_OUTPUT,EQ_MIDSIDE,-1,-1,-1},{"Input","Output","Mid/Side","","",""},{"DB","DB"}},
    {"ROUTING",1,{EQ_PARALLEL,-1,-1,-1,-1,-1},{"Parallel","","","","",""}, {}}
};
static constexpr FlavorBank eq_band[] = {
    {"SHAPE",0,{EB_FREQ,EB_GAIN,EB_Q,-1,-1,-1},{"Frequency","Gain","Q","","",""},{"HZ","DB"}},
    {"MODE",1,{EB_TYPE,EB_ON,EB_BYPASS,-1,-1,-1},{"Type","Band On","Band Bypass","","",""}, {}},
    {"FILTER",2,{EB_FAMILY,EB_SLOPE,EB_BELL,-1,-1,-1},{"Filter Family","Slope","Bell Shape","","",""}, {}},
    {"STEREO",3,{EB_STEREO,-1,-1,-1,-1,-1},{"Position","","","","",""}, {}}
};
