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
    float pot1 = hw.feedback_knob.Process(); // Pot1
    float pot2 = hw.mix_knob.Process();      // Pot2
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

    // Enc1 scrolls params
    int32_t enc1_inc = hw.time_encoder.Increment();
    if(enc1_inc != 0)
    {
        selected_param += (enc1_inc > 0 ? 1 : -1);
        if(selected_param < 0)
            selected_param = PARAM_COUNT - 1;
        if(selected_param >= PARAM_COUNT)
            selected_param = 0;
    }

    // Enc2 edits selected param
    int32_t enc2_inc = hw.aux_encoder.Increment();
    if(enc2_inc != 0)
    {
        switch(selected_param)
        {
            case PARAM_MIX:
                dry_wet_mix = fclamp(dry_wet_mix + enc2_inc * 0.01f, 0.0f, 1.0f);
                break;
            case PARAM_FEEDBACK:
                feedback = fclamp(feedback + enc2_inc * 0.01f, 0.0f, 0.98f);
                break;
            case PARAM_DELAY: {
                float scale_factor = 500.0f;
                time_as_float += enc2_inc * scale_factor;
            } break;
            default: break;
        }
    }

    // Latch encoder clicks as events to be handled in main loop/UI
    if(hw.time_encoder.RisingEdge() || hw.aux_encoder.RisingEdge())
    {
        enc_click_pending = true;
        enc_click_time    = System::GetNow();
    }

    // Hold-to-assign: long hold Enc1 assigns Pot1; long hold Enc2 assigns Pot2 to current param
    const uint32_t hold_ms = 600;
    if(hw.time_encoder.Pressed() && hw.time_encoder.TimeHeldMs() >= hold_ms)
        master_of_param[selected_param] = 1;
    if(hw.aux_encoder.Pressed() && hw.aux_encoder.TimeHeldMs() >= hold_ms)
        master_of_param[selected_param] = 2;

    // Apply pots to their assigned params
    for(int p = 0; p < PARAM_COUNT; ++p)
    {
        int m = master_of_param[p];
        if(m == 0)
            continue;
        float v = (m == 1) ? pot1 : pot2;
        switch(p)
        {
            case PARAM_MIX:      dry_wet_mix = v; break;
            case PARAM_FEEDBACK: feedback = v * 0.98f; break;
            case PARAM_DELAY: {
                // Map pot [0..1] to useful delay range
                float min_delay = 100.0f;
                float max_delay = (float)MAX_DELAY - 4.f;
                time_as_float   = fclamp(min_delay + v * (max_delay - min_delay), min_delay, max_delay);
            } break;
            default: break;
        }
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


