#include "hw.h"

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // --- ADC Configuration for Potentiometer ---
    // 1. Define the config
    AdcChannelConfig adc_config;
    // Configure Pin 16 (A1) as an ADC input
    adc_config.InitSingle(seed.GetPin(15)); 

    // 2. Initialize the ADC peripheral with this config
    seed.adc.Init(&adc_config, 1);
    
    // 3. Start the ADC so it begins reading data into memory
    seed.adc.Start();

    // 4. Bind the AnalogControl to the memory address where the ADC writes data
    // seed.adc.GetPtr(0) returns the address of the data for the first channel (index 0)
    pot.Init(seed.adc.GetPtr(0), seed.AudioCallbackRate());

    // --- Encoder ---
    encoder.Init(seed.GetPin(1), seed.GetPin(28), seed.GetPin(2), seed.AudioCallbackRate());

    // --- Button (D18) ---
    button.Init(seed.GetPin(18), seed.AudioCallbackRate());
}