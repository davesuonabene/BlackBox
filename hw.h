#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// 20 seconds @ 48kHz = 960,000 samples
#define LOOPER_MAX_SAMPLES 960000

struct Hardware
{
    DaisySeed seed;
    Encoder   encoder;
    Switch    button;
    AnalogControl pot;

    float     sample_rate;

    // --- Looper Data ---
    // Buffer placed in external SDRAM
    static float DSY_SDRAM_BSS loop_buffer[LOOPER_MAX_SAMPLES];
    
    enum LooperMode { LP_EMPTY, LP_RECORDING, LP_PLAYING };
    LooperMode looper_mode = LP_EMPTY;
    
    uint32_t loop_length = 0;
    uint32_t loop_pos = 0;

    void Init();
};