#include "hw.h"

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // --- ADC Configuration (Potentiometer) ---
    AdcChannelConfig adc_config;
    // Configure Pin 15 (D15) as ADC Input
    adc_config.InitSingle(seed.GetPin(15)); 

    // Initialize and Start ADC
    seed.adc.Init(&adc_config, 1);
    seed.adc.Start();

    // Bind AnalogControl to the ADC buffer
    pot.Init(seed.adc.GetPtr(0), seed.AudioCallbackRate());

    // --- Controls ---
    encoder.Init(seed.GetPin(1), seed.GetPin(28), seed.GetPin(2), seed.AudioCallbackRate());
    button.Init(seed.GetPin(18), seed.AudioCallbackRate());
}