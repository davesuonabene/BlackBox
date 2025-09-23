#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

struct Hardware
{
    DaisySeed    seed;
    AnalogControl feedback_knob;
    AnalogControl mix_knob;
    Encoder        time_encoder;
    Encoder        aux_encoder;
    Switch         tap_switch;
    Switch         lfo_switch;
    Switch         clear_switch;
    Led            tempo_led;

    float          sample_rate;

    void Init();
};


