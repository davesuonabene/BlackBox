#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"
#include "daisy_seed.h"

struct Processing
{
    daisysp::Oscillator osc;
    volatile float      frequency;
    volatile bool       muted;

    // For velocity control
    uint32_t last_turn_time;
    float    current_step;
    int8_t   last_direction;

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void Process(float &outl, float &outr, float inl, float inr);
};