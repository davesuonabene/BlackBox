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
    int32_t  aux_count     = 0;
    bool     enc_click_pending = false;
    uint32_t enc_click_time    = 0;

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void GetDelaySample(float &outl, float &outr, float inl, float inr, float sample_rate);
};


