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
    enum Param { PARAM_MIX = 0, PARAM_FEEDBACK, PARAM_DELAY, PARAM_COUNT };
    int selected_param = PARAM_MIX;
    // 0 = none, 1 = Pot1 (feedback_knob), 2 = Pot2 (mix_knob)
    int master_of_param[PARAM_COUNT] = {0, 0, 0};

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void GetDelaySample(float &outl, float &outr, float inl, float inr, float sample_rate);
};


