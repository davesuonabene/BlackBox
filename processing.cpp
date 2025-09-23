#include "processing.h"
using namespace daisy;
using namespace daisysp;

DelayLine<float, MAX_DELAY> Processing::dell;
DelayLine<float, MAX_DELAY> Processing::delr;

void Processing::Init(Hardware &hw)
{
    dell.Init();
    delr.Init();

    osc.Init(hw.sample_rate);
    osc.SetWaveform(Oscillator::WAVE_SIN);

    time_as_float = hw.sample_rate * 0.5f;
    fonepole(current_delay, time_as_float, 1.f);
    dell.SetDelay(current_delay);
    delr.SetDelay(current_delay);
}

void Processing::Controls(Hardware &hw)
{
    feedback    = hw.feedback_knob.Process() * 0.98f;
    dry_wet_mix = hw.mix_knob.Process();
    hw.time_encoder.Debounce();
    hw.aux_encoder.Debounce();
    hw.tap_switch.Debounce();
    hw.lfo_switch.Debounce();
    hw.clear_switch.Debounce();

    if(hw.lfo_switch.RisingEdge())
        lfo_active = !lfo_active;

    is_clearing = hw.clear_switch.Pressed();
    if(hw.clear_switch.RisingEdge())
    {
        dell.Reset();
        delr.Reset();
    }

    if(hw.tap_switch.RisingEdge())
    {
        uint32_t now      = System::GetNow();
        uint32_t tap_diff = now - last_tap_time;
        if(tap_diff < MAX_TAP_INTERVAL)
            time_as_float = static_cast<float>(tap_diff) * 0.001f * hw.sample_rate;
        last_tap_time = now;
    }

    int32_t encoder_inc = hw.time_encoder.Increment();
    if(encoder_inc != 0)
    {
        float scale_factor = 500.0f;
        time_as_float += encoder_inc * scale_factor;
    }

    // Aux encoder increments counter
    int32_t aux_inc = hw.aux_encoder.Increment();
    if(aux_inc != 0)
        aux_count += aux_inc;

    // Latch encoder clicks as events to be handled in main loop/UI
    if(hw.time_encoder.RisingEdge() || hw.aux_encoder.RisingEdge())
    {
        enc_click_pending = true;
        enc_click_time    = System::GetNow();
    }

    float min_delay = 100.0f;
    time_as_float   = fclamp(time_as_float, min_delay, MAX_DELAY - 4.f);
    if(time_as_float > 0.f)
    {
        float osc_freq = (hw.sample_rate / time_as_float) * 4.0f;
        osc.SetFreq(osc_freq);
    }
}

void Processing::GetDelaySample(float &outl,
                                float &outr,
                                float  inl,
                                float  inr,
                                float  sample_rate)
{
    if(is_clearing)
    {
        dell.Write(0.f);
        delr.Write(0.f);
        outl = inl;
        outr = inr;
        return;
    }

    fonepole(current_delay, time_as_float, .0002f);
    dell.SetDelay(current_delay);
    delr.SetDelay(current_delay);

    float wetl = dell.Read();
    float wetr = delr.Read();

    if(lfo_active)
    {
        float osc_mod = (osc.Process() + 1.0f) * 0.5f;
        wetl *= osc_mod;
        wetr *= osc_mod;
    }

    float feedback_l = inl + (wetl * feedback);
    float feedback_r = inr + (wetr * feedback);

    dell.Write(feedback_l);
    delr.Write(feedback_r);

    outl = (wetl * dry_wet_mix) + (inl * (1.0f - dry_wet_mix));
    outr = (wetr * dry_wet_mix) + (inr * (1.0f - dry_wet_mix));
}


