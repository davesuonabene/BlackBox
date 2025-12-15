#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

struct Hardware
{
    DaisySeed seed;
    Encoder   encoder;
    Switch    button;
    AnalogControl pot;

    float     sample_rate;

    void Init();
};