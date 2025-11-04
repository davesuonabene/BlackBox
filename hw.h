#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

struct Hardware
{
    DaisySeed seed;
    Encoder   encoder;

    float     sample_rate;

    void Init();
};