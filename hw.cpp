#include "hw.h"

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // Single encoder
    // Revert update rate to AudioCallbackRate
    encoder.Init(seed.GetPin(1), seed.GetPin(28), seed.GetPin(2), seed.AudioCallbackRate());
}