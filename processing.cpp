#include "processing.h"
#include <string.h> 
#include <math.h>   
#include <stdlib.h> 
#include <cstdio> // for snprintf

using namespace daisy;
using namespace daisysp;

// --- Menu Tree Definition ---

// 1. Generic Edit Menu
// Order: Map, Back (Separator drawn in screen.cpp)
const MenuItem kMenuGenericEdit[] = {
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT, nullptr, 0}, 
    {"< Back",  TYPE_BACK,  0,             kMenuMain, 0},
};
const int kMenuGenericEditSize = sizeof(kMenuGenericEdit) / sizeof(kMenuGenericEdit[0]);

// 2. Post Gain Submenu (Pre, Send)
// Order: Map, Back, (Separator), Pre, Send
const MenuItem kMenuPostEdit[] = {
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT,  nullptr, 0}, 
    {"< Back",  TYPE_BACK,  0,              kMenuMain, 0},
    {"Pre",     TYPE_PARAM, PARAM_PRE_GAIN, nullptr, 0},
    {"Send",    TYPE_PARAM, PARAM_SEND,     nullptr, 0},
};
const int kMenuPostEditSize = sizeof(kMenuPostEdit) / sizeof(kMenuPostEdit[0]);

// 3. BPM Edit Menu
// Order: Map, Back, (Separator), Div
const MenuItem kMenuBpmEdit[] = {
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT,  nullptr, 0},
    {"< Back",  TYPE_BACK,  0,              kMenuMain, 0},
    {"Div",     TYPE_PARAM, PARAM_DIVISION, nullptr, 0},
};
const int kMenuBpmEditSize = sizeof(kMenuBpmEdit) / sizeof(kMenuBpmEdit[0]);

// 4. Main Menu
// Fbk and Mix are back here. Post controls Pre/Send via submenu.
const MenuItem kMenuMain[] = {
    {"Post",    TYPE_PARAM_SUBMENU, PARAM_POST_GAIN,    kMenuPostEdit,  kMenuPostEditSize},
    {"Fbk",     TYPE_PARAM,         PARAM_FEEDBACK,     nullptr,        0},
    {"Mix",     TYPE_PARAM,         PARAM_MIX,          nullptr,        0},
    {"BPM",     TYPE_PARAM_SUBMENU, PARAM_BPM,          kMenuBpmEdit,   kMenuBpmEditSize},
    {"Pitch",   TYPE_PARAM,         PARAM_PITCH,        nullptr,        0},
    {"Size",    TYPE_PARAM,         PARAM_GRAIN_SIZE,   nullptr,        0},
    {"Density", TYPE_PARAM,         PARAM_GRAIN_DENSITY,nullptr,        0},
    {"Stereo",  TYPE_PARAM,         PARAM_STEREO,       nullptr,        0},
};
const int kMenuMainSize = sizeof(kMenuMain) / sizeof(kMenuMain[0]);


// --- Static Member Definitions ---
float DSY_SDRAM_BSS Processing::buffer[MAX_BUFFER_SAMPLES];
Processing::Grain Processing::grains_l[MAX_GRAINS];
Processing::Grain Processing::grains_r[MAX_GRAINS];

void Processing::Init(Hardware &hw)
{
    memset(buffer, 0, MAX_BUFFER_SAMPLES * sizeof(float));
    sample_rate_ = hw.sample_rate;

    // Default Params
    params[PARAM_PRE_GAIN]      = 0.5f; 
    params[PARAM_SEND]          = 1.0f;
    params[PARAM_FEEDBACK]      = 0.5f;
    params[PARAM_MIX]           = 0.5f;
    params[PARAM_POST_GAIN]     = 0.5f;
    params[PARAM_BPM]           = 120.0f;
    params[PARAM_DIVISION]      = 1.0f; 
    params[PARAM_PITCH]         = 1.0f; 
    params[PARAM_GRAIN_SIZE]    = 0.1f;
    params[PARAM_GRAIN_DENSITY] = 10.0f;
    params[PARAM_STEREO]        = 0.0f;

    // Initialize Maps to 0
    for(int i=0; i<PARAM_COUNT; i++) {
        knob_map_amounts[i] = 0.0f;
        effective_params[i] = params[i];
    }
    
    // Default parent name
    snprintf(parent_menu_name, sizeof(parent_menu_name), " ");

    division_idx = 0; 
    params[PARAM_DIVISION] = (float)division_vals[division_idx];

    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::Controls(Hardware &hw)
{
    // --- Potentiometer Logic ---
    hw.pot.Process();
    float pot_val = hw.pot.Value();

    // Calculate Effective Params
    for (int i = 0; i < PARAM_COUNT; i++)
    {
        if (i == PARAM_DIVISION || i == PARAM_MAP_AMT) continue;

        float base = params[i];
        float map  = knob_map_amounts[i]; 
        
        float range = 1.0f; 
        float min_val = 0.0f;
        float max_val = 1.0f;

        switch(i)
        {
            case PARAM_BPM: 
                min_val = 20.0f; max_val = 300.0f; range = 280.0f; break;
            case PARAM_PITCH: 
                min_val = 0.25f; max_val = 4.0f; range = 3.75f; break; 
            case PARAM_GRAIN_SIZE:
                min_val = 0.002f; max_val = 0.5f; range = 0.498f; break;
            case PARAM_GRAIN_DENSITY:
                min_val = 0.5f; max_val = 50.0f; range = 49.5f; break;
            default: 
                min_val = 0.0f; max_val = 1.0f; range = 1.0f; break;
        }

        float mod = pot_val * map * range;
        effective_params[i] = fclamp(base + mod, min_val, max_val);
    }
    
    UpdateBufferLen(); 

    // --- Button Logic ---
    hw.button.Debounce();
    if(hw.button.RisingEdge())
    {
        trigger_blink = true;
    }

    // --- Encoder Logic ---
    hw.encoder.Debounce();
    int32_t inc = hw.encoder.Increment();

    if(hw.encoder.RisingEdge())
    {
        enc_hold_start = System::GetNow();
        enc_is_holding = true;
    }

    if(enc_is_holding && hw.encoder.TimeHeldMs() >= kHoldTimeMs)
    {
        enc_is_holding = false; 
        
        const MenuItem& item = GetSelectedItem();
        if (ui_state == STATE_MENU_NAV)
        {
            // Capture Parent Name for Header
            snprintf(parent_menu_name, sizeof(parent_menu_name), "%s", item.name);

            if(item.type == TYPE_PARAM)
            {
                edit_param_target = item.param_id; 
                current_menu = kMenuGenericEdit;
                current_menu_size = kMenuGenericEditSize;
                selected_item_idx = 0;
                view_top_item_idx = 0;
            }
            else if(item.type == TYPE_PARAM_SUBMENU)
            {
                edit_param_target = item.param_id;
                current_menu = item.submenu;
                current_menu_size = item.num_children;
                selected_item_idx = 0;
                view_top_item_idx = 0;
            }
        }
    }

    if(hw.encoder.FallingEdge())
    {
        if(enc_is_holding) 
        {
            enc_is_holding = false;
            const MenuItem& item = GetSelectedItem();

            if (ui_state == STATE_MENU_NAV)
            {
                switch(item.type)
                {
                    case TYPE_PARAM:
                    case TYPE_PARAM_SUBMENU: 
                        ui_state = STATE_PARAM_EDIT;
                        break;
                    case TYPE_SUBMENU:
                        current_menu = item.submenu;
                        current_menu_size = item.num_children;
                        selected_item_idx = 0;
                        view_top_item_idx = 0;
                        break;
                    case TYPE_BACK:
                        current_menu = item.submenu;
                        if (current_menu == kMenuMain)
                            current_menu_size = kMenuMainSize;
                        else
                            current_menu_size = 0; 
                        
                        selected_item_idx = 0;
                        view_top_item_idx = 0;
                        break;
                }
            }
            else
            {
                ui_state = STATE_MENU_NAV;
            }
        }
    }

    if(inc != 0)
    {
        if(ui_state == STATE_MENU_NAV)
        {
            selected_item_idx += inc;
            if(selected_item_idx < 0) selected_item_idx = current_menu_size - 1;
            if(selected_item_idx >= current_menu_size) selected_item_idx = 0;

            const int max_lines = 4; 
            if(selected_item_idx < view_top_item_idx)
                view_top_item_idx = selected_item_idx;
            else if(selected_item_idx >= view_top_item_idx + max_lines)
                view_top_item_idx = selected_item_idx - (max_lines - 1);
        }
        else
        {
            int param_id = GetSelectedItem().param_id;
            
            if (param_id == PARAM_MAP_AMT)
            {
                float val = knob_map_amounts[edit_param_target];
                val += (float)inc * 0.05f; 
                knob_map_amounts[edit_param_target] = fclamp(val, -1.0f, 1.0f);
            }
            else
            {
                float val = params[param_id];
                float delta = 0.01f * inc; 
                
                switch(param_id)
                {
                    case PARAM_PRE_GAIN:
                    case PARAM_POST_GAIN:
                    case PARAM_SEND:
                    case PARAM_FEEDBACK:
                    case PARAM_MIX:
                    case PARAM_STEREO:
                        params[param_id] = fclamp(val + delta, 0.0f, 1.0f);
                        break;
                    case PARAM_BPM:
                        params[param_id] = fclamp(val + (delta * 100.0f), 20.0f, 300.0f);
                        break;
                    case PARAM_DIVISION:
                        division_idx += inc > 0 ? 1 : -1;
                        if(division_idx < 0) division_idx = 3;
                        if(division_idx > 3) division_idx = 0;
                        params[param_id] = (float)division_vals[division_idx];
                        break;
                    case PARAM_PITCH: 
                        val = 12.0f * log2f(val); 
                        val += (delta * 100.0f); 
                        val = fclamp(val, -24.0f, 24.0f); 
                        params[param_id] = powf(2.0f, val / 12.0f); 
                        break;
                    case PARAM_GRAIN_SIZE: 
                    {
                        float vel_mod = fminf((float)abs(inc) * 0.5f, 5.0f);
                        float fine_delta = (inc > 0 ? 1.0f : -1.0f) * (0.001f + (0.005f * vel_mod));
                        params[param_id] = fclamp(val + fine_delta, 0.002f, 0.5f);
                    }
                        break;
                    case PARAM_GRAIN_DENSITY: 
                        params[param_id] = fclamp(val + (delta * 10.0f), 0.5f, 50.0f);
                        UpdateGrainParams();
                        break;
                    default: break;
                }
            }
        }
    }
}

// --- Audio Functions ---
void Processing::UpdateBufferLen()
{
    float bpm       = effective_params[PARAM_BPM];
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
    float density_hz = effective_params[PARAM_GRAIN_DENSITY];
    float stereo_amt = effective_params[PARAM_STEREO];
    if(density_hz < 0.1f) density_hz = 0.1f;
    float base_interval = sample_rate_ / density_hz;
    float l_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt);
    float r_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt);
    grain_trig_interval_l = (uint32_t)(base_interval * l_rand);
    grain_trig_interval_r = (uint32_t)(base_interval * r_rand);
    if(grain_trig_interval_l == 0) grain_trig_interval_l = 1;
    if(grain_trig_interval_r == 0) grain_trig_interval_r = 1;
}

void Processing::GetSample(float &outl, float &outr, float inl, float inr)
{
    float pre_gain  = effective_params[PARAM_PRE_GAIN] * 2.0f; 
    float send      = effective_params[PARAM_SEND];
    float fbk       = effective_params[PARAM_FEEDBACK];
    float mix       = effective_params[PARAM_MIX];
    float post_gain = effective_params[PARAM_POST_GAIN] * 2.0f;
    float stereo    = effective_params[PARAM_STEREO];
    
    float inl_gained = inl * pre_gain;
    float inr_gained = inr * pre_gain;
    
    float wet_in = (inl_gained + inr_gained) * 0.5f * send;
    float old_samp = buffer[write_pos];
    
    buffer[write_pos] = fclamp(wet_in + (old_samp * fbk), -1.0f, 1.0f);
    
    if(grain_trig_counter_l == 0)
    {
        float size_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t size_samps = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * size_mod);
        for(int i = 0; i < MAX_GRAINS; i++)
        {
            if(!grains_l[i].active)
            {
                grains_l[i].Start(write_pos, effective_params[PARAM_PITCH], size_samps, sample_rate_);
                break;
            }
        }
        UpdateGrainParams(); 
        grain_trig_counter_l = grain_trig_interval_l;
    }
    grain_trig_counter_l--;
    
    if(grain_trig_counter_r == 0)
    {
        float size_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t size_samps = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * size_mod);
        for(int i = 0; i < MAX_GRAINS; i++)
        {
            if(!grains_r[i].active)
            {
                grains_r[i].Start(write_pos, effective_params[PARAM_PITCH], size_samps, sample_rate_);
                break;
            }
        }
        grain_trig_counter_r = grain_trig_interval_r;
    }
    grain_trig_counter_r--;
    
    float wet_l = 0.0f;
    float wet_r = 0.0f;
    for(int i = 0; i < MAX_GRAINS; i++)
    {
        wet_l += grains_l[i].Process(buffer, buffer_len_samples);
        wet_r += grains_r[i].Process(buffer, buffer_len_samples);
    }
    wet_l *= 0.5f; 
    wet_r *= 0.5f;
    write_pos++;
    if(write_pos >= buffer_len_samples)
        write_pos = 0;
    
    float dry_l = inl_gained * (1.0f - mix);
    float dry_r = inr_gained * (1.0f - mix);
    
    float wet_l_mixed = wet_l * mix;
    float wet_r_mixed = wet_r * mix;
    
    outl = (dry_l + wet_l_mixed) * post_gain;
    outr = (dry_r + wet_r_mixed) * post_gain;
}