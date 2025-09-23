#include "hw.h"

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    AdcChannelConfig adcConfig[2];
    adcConfig[0].InitSingle(seed.GetPin(15));
    adcConfig[1].InitSingle(seed.GetPin(16));
    seed.adc.Init(adcConfig, 2);
    seed.adc.Start();

    feedback_knob.Init(seed.adc.GetPtr(0), sample_rate);
    mix_knob.Init(seed.adc.GetPtr(1), sample_rate);

    time_encoder.Init(seed.GetPin(5), seed.GetPin(6), seed.GetPin(4), seed.AudioCallbackRate());
    aux_encoder.Init(seed.GetPin(8), seed.GetPin(7), seed.GetPin(9), seed.AudioCallbackRate());

    lfo_switch.Init(seed.GetPin(1), seed.AudioCallbackRate());
    clear_switch.Init(seed.GetPin(2), seed.AudioCallbackRate());
    tap_switch.Init(seed.GetPin(3), seed.AudioCallbackRate());

    tempo_led.Init(seed.GetPin(21), false);
}


