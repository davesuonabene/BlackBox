#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"

struct Processing
{
    // Delay lines
    static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
    static DelayLine<float, MAX_DELAY> delr;

    // Signal components
    Oscillator osc;

    // State
    float    current_delay = 0.0f;
    float    feedback      = 0.0f;
    float    dry_wet_mix   = 0.0f;
    float    time_as_float = 0.0f;
    uint32_t last_tap_time = 0;
    bool     lfo_active    = false;
    bool     is_clearing   = false;
    bool     enc_click_pending = false;
    uint32_t enc_click_time    = 0;

    // UI: param selection and pot-master mapping
    enum Param { PARAM_MIX = 0, PARAM_FEEDBACK, PARAM_BPM, PARAM_DIVISION, PARAM_COUNT };
    int selected_param = PARAM_MIX;
    // 0 = none, 1 = Pot1 (feedback_knob), 2 = Pot2 (mix_knob)
    int master_of_param[PARAM_COUNT] = {0, 0, 0, 0};

    // Debounce flags for long-hold assignment toggle per encoder
    bool hold_assigned_time = false;
    bool hold_assigned_aux  = false;

    // Tempo in beats per minute for quarter-note delay
    float bpm = 120.0f;

    // Division denominator: 1 (quarter), 2 (eighth), 3 (triplet), 4 (sixteenth)
    int division = 1;

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void GetDelaySample(float &outl, float &outr, float inl, float inr, float sample_rate);
};


