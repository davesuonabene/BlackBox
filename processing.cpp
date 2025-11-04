#include "processing.h"
#include <string.h> // for memset
#include <math.h>   // For log2f, powf, fabsf
#include <stdlib.h> // For abs()

using namespace daisy;
using namespace daisysp;

// Define static members
float DSY_SDRAM_BSS Processing::buffer[MAX_BUFFER_SAMPLES];
Processing::Grain Processing::grains_l[MAX_GRAINS];
Processing::Grain Processing::grains_r[MAX_GRAINS];

void Processing::Init(Hardware &hw)
{
    memset(buffer, 0, MAX_BUFFER_SAMPLES * sizeof(float));
    sample_rate_ = hw.sample_rate;

    // Default Params
    params[PARAM_PRE_GAIN]      = 0.5f; // 0-2 range, 0.5 = 100% (0dB)
    params[PARAM_SEND]          = 1.0f; // 0-1 range
    params[PARAM_FEEDBACK]      = 0.5f; // 0-1 range
    params[PARAM_MIX]           = 0.5f; // 0-1 range
    params[PARAM_POST_GAIN]     = 0.5f; // 0-2 range, 0.5 = 100% (0dB)
    params[PARAM_BPM]           = 120.0f;
    params[PARAM_DIVISION]      = 1.0f; 
    params[PARAM_PITCH]         = 1.0f; 
    params[PARAM_GRAIN_SIZE]    = 0.1f; // 100ms
    params[PARAM_GRAIN_DENSITY] = 10.0f; // 10 Hz
    params[PARAM_STEREO]        = 0.0f; // 0-1 range

    division_idx = 0; // 1/4 note
    params[PARAM_DIVISION] = (float)division_vals[division_idx];

    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::UpdateBufferLen()
{
    float bpm       = params[PARAM_BPM];
    float division  = params[PARAM_DIVISION];
    float beats_per_sec = bpm / 60.0f;
    float beat_len_sec  = 1.0f / beats_per_sec;
    float loop_len_sec  = beat_len_sec * (4.0f / division);
    
    buffer_len_samples = (uint32_t)(loop_len_sec * sample_rate_);
    if(buffer_len_samples > MAX_BUFFER_SAMPLES)
        buffer_len_samples = MAX_BUFFER_SAMPLES;
    if(buffer_len_samples < 4) 
        buffer_len_samples = 4;
        
    write_pos = 0; 
}

void Processing::UpdateGrainParams()
{
    float density_hz = params[PARAM_GRAIN_DENSITY];
    float stereo_amt = params[PARAM_STEREO];

    if(density_hz < 0.1f) density_hz = 0.1f;
    
    // Base interval
    float base_interval = sample_rate_ / density_hz;
    
    // Randomize L/R intervals based on stereo param
    float l_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt);
    float r_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt);

    grain_trig_interval_l = (uint32_t)(base_interval * l_rand);
    grain_trig_interval_r = (uint32_t)(base_interval * r_rand);
    
    if(grain_trig_interval_l == 0) grain_trig_interval_l = 1;
    if(grain_trig_interval_r == 0) grain_trig_interval_r = 1;
}

void Processing::Controls(Hardware &hw)
{
    hw.encoder.Debounce();

    int32_t inc = hw.encoder.Increment();

    if(hw.encoder.RisingEdge())
    {
        enc_click_pending = true;
        ui_state = (ui_state == STATE_NAVIGATE) ? STATE_EDIT : STATE_NAVIGATE;
    }

    if(inc != 0)
    {
        if(ui_state == STATE_NAVIGATE)
        {
            selected_param += inc;
            if(selected_param < 0)
                selected_param = PARAM_COUNT - 1;
            if(selected_param >= PARAM_COUNT)
                selected_param = 0;

            const int max_lines = 5; 
            if(selected_param < view_top_param)
            {
                view_top_param = selected_param;
            }
            else if(selected_param >= view_top_param + max_lines)
            {
                view_top_param = selected_param - (max_lines - 1);
            }
        }
        else // STATE_EDIT
        {
            float val   = params[selected_param];
            // Standard delta for most params
            float delta = 0.01f * inc; 
            
            switch(selected_param)
            {
                // 0-200% params (stored 0-1)
                case PARAM_PRE_GAIN:
                case PARAM_POST_GAIN:
                    params[selected_param] = fclamp(val + delta, 0.0f, 1.0f);
                    break;
                // 0-100% params (stored 0-1)
                case PARAM_SEND:
                case PARAM_FEEDBACK:
                case PARAM_MIX:
                case PARAM_STEREO:
                    params[selected_param] = fclamp(val + delta, 0.0f, 1.0f);
                    break;
                case PARAM_BPM:
                    params[selected_param] = fclamp(val + (delta * 100.0f), 20.0f, 300.0f);
                    UpdateBufferLen();
                    break;
                case PARAM_DIVISION:
                    division_idx += inc > 0 ? 1 : -1;
                    if(division_idx < 0) division_idx = 3;
                    if(division_idx > 3) division_idx = 0;
                    params[PARAM_DIVISION] = (float)division_vals[division_idx];
                    UpdateBufferLen();
                    break;
                case PARAM_PITCH: 
                    val = 12.0f * log2f(val); 
                    val += (delta * 100.0f); 
                    val = fclamp(val, -24.0f, 24.0f); 
                    params[selected_param] = powf(2.0f, val / 12.0f); 
                    break;
                case PARAM_GRAIN_SIZE: 
                {
                    // Finer control: delta scales with encoder velocity
                    float vel_mod = fminf((float)abs(inc) * 0.5f, 5.0f); // Scale velocity
                    float fine_delta = (inc > 0 ? 1.0f : -1.0f) * (0.001f + (0.005f * vel_mod));
                    params[selected_param] = fclamp(val + fine_delta, 0.002f, 0.5f); // Min 2ms
                }
                    break;
                case PARAM_GRAIN_DENSITY: 
                    params[selected_param] = fclamp(val + (delta * 10.0f), 0.5f, 50.0f);
                    UpdateGrainParams(); // Update intervals on change
                    break;
                default: break;
            }
        }
    }
}

void Processing::GetSample(float &outl, float &outr, float inl, float inr)
{
    float pre_gain  = params[PARAM_PRE_GAIN] * 2.0f; // 0..2
    float send      = params[PARAM_SEND];
    float fbk       = params[PARAM_FEEDBACK];
    float mix       = params[PARAM_MIX];
    float post_gain = params[PARAM_POST_GAIN] * 2.0f; // 0..2
    float stereo    = params[PARAM_STEREO];

    // --- Input Stage ---
    float inl_gained = inl * pre_gain;
    float inr_gained = inr * pre_gain;

    // Signal sent to granulator buffer (mono)
    float wet_in = (inl_gained + inr_gained) * 0.5f * send;

    // --- Record ---
    float old_samp = buffer[write_pos];
    buffer[write_pos] = fclamp(wet_in + (old_samp * fbk), -1.0f, 1.0f);

    // --- Trigger Grains (L) ---
    if(grain_trig_counter_l == 0)
    {
        float size_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t size_samps = (uint32_t)(params[PARAM_GRAIN_SIZE] * sample_rate_ * size_mod);
        
        for(int i = 0; i < MAX_GRAINS; i++)
        {
            if(!grains_l[i].active)
            {
                grains_l[i].Start(write_pos, params[PARAM_PITCH], size_samps, sample_rate_);
                break;
            }
        }
        UpdateGrainParams(); // Re-roll random intervals
        grain_trig_counter_l = grain_trig_interval_l;
    }
    grain_trig_counter_l--;

    // --- Trigger Grains (R) ---
    if(grain_trig_counter_r == 0)
    {
        float size_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t size_samps = (uint32_t)(params[PARAM_GRAIN_SIZE] * sample_rate_ * size_mod);
        
        for(int i = 0; i < MAX_GRAINS; i++)
        {
            if(!grains_r[i].active)
            {
                grains_r[i].Start(write_pos, params[PARAM_PITCH], size_samps, sample_rate_);
                break;
            }
        }
        grain_trig_counter_r = grain_trig_interval_r;
    }
    grain_trig_counter_r--;


    // --- Process Grains ---
    float wet_l = 0.0f;
    float wet_r = 0.0f;
    for(int i = 0; i < MAX_GRAINS; i++)
    {
        wet_l += grains_l[i].Process(buffer, buffer_len_samples);
        wet_r += grains_r[i].Process(buffer, buffer_len_samples);
    }
    wet_l *= 0.5f; // Attenuate
    wet_r *= 0.5f;

    // --- Advance Write Head ---
    write_pos++;
    if(write_pos >= buffer_len_samples)
        write_pos = 0;

    // --- Mix Stage ---
    float dry_l = inl_gained * (1.0f - mix);
    float dry_r = inr_gained * (1.0f - mix);

    float wet_l_mixed = wet_l * mix;
    float wet_r_mixed = wet_r * mix;

    // --- Output Stage ---
    outl = (dry_l + wet_l_mixed) * post_gain;
    outr = (dry_r + wet_r_mixed) * post_gain;
}