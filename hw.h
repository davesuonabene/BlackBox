#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

struct Hardware
{
    DaisySeed     seed;
    
    // 1 Knob, 1 Encoder, 1 Button
    AnalogControl pot;
    Encoder       encoder;
    Switch        button;
    
    float         sample_rate;

    void Init();
};