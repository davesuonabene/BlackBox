#include "hw.h"

// Definition of the static loop buffers
float DSY_SDRAM_BSS Hardware::buffer_a[LOOPER_MAX_SAMPLES];
float DSY_SDRAM_BSS Hardware::buffer_b[LOOPER_MAX_SAMPLES];

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // --- ADC Configuration ---
    AdcChannelConfig adc_config;
    adc_config.InitSingle(seed.GetPin(15)); 
    seed.adc.Init(&adc_config, 1);
    seed.adc.Start();
    pot.Init(seed.adc.GetPtr(0), seed.AudioCallbackRate(), true);

    // --- Controls ---
    encoder.Init(seed.GetPin(1), seed.GetPin(28), seed.GetPin(2), seed.AudioCallbackRate());
    button.Init(seed.GetPin(18), seed.AudioCallbackRate());

    // --- Looper Init ---
    Reset();
}