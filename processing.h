#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"

using namespace daisy;
using namespace daisysp;

struct Processing
{
    // Nested struct for Grain
    struct Grain
    {
        // Simple triangular envelope
        inline float TriEnv(float pos)
        {
            if(pos < 0.5f)
                return pos * 2.0f; // Attack
            return (1.0f - pos) * 2.0f; // Decay
        }

        bool     active = false;
        float    read_pos;
        float    increment; // For pitch
        float    env_pos;
        float    env_inc; // For size
        uint32_t size_samples;

        void Start(float start_pos, float pitch, uint32_t size_samps, float sample_rate)
        {
            active       = true;
            read_pos     = start_pos;
            increment    = pitch;
            size_samples = size_samps < 4 ? 4 : size_samps;
            env_pos      = 0.0f;
            env_inc      = 1.0f / (float)size_samples;
        }

        float Process(float *buffer, size_t buffer_len)
        {
            if(!active)
                return 0.0f;

            // Simple linear interpolation
            int32_t i_idx  = (int32_t)read_pos;
            float   frac   = read_pos - i_idx;
            float   samp_a = buffer[i_idx];
            float   samp_b = buffer[(i_idx + 1) % buffer_len];
            float   samp   = samp_a + (samp_b - samp_a) * frac;

            float amp = TriEnv(env_pos);
            
            read_pos += increment;
            while(read_pos >= buffer_len)
                read_pos -= buffer_len;
            while(read_pos < 0)
                read_pos += buffer_len;

            env_pos += env_inc;
            if(env_pos >= 1.0f)
                active = false;

            return samp * amp;
        }
    };

    // Simple 0-1 float random number generator
    struct Rand
    {
        uint32_t seed_ = 1;
        float Process()
        {
            seed_ = (seed_ * 1664525L + 1013904223L) & 0xFFFFFFFF;
            return (float)seed_ / 4294967295.0f;
        }
    };


    enum Param
    {
        PARAM_PRE_GAIN,
        PARAM_SEND,
        PARAM_FEEDBACK,
        PARAM_MIX,
        PARAM_POST_GAIN,
        PARAM_BPM,
        PARAM_DIVISION,
        PARAM_PITCH,
        PARAM_GRAIN_SIZE,
        PARAM_GRAIN_DENSITY,
        PARAM_STEREO,
        PARAM_COUNT
    };

    const char *param_names[PARAM_COUNT] = {
        "Pre",
        "Send",
        "Fbk",
        "Mix",
        "Post",
        "BPM",
        "Div",
        "Pitch",
        "Size",
        "Density",
        "Stereo",
    };

    enum UiState
    {
        STATE_NAVIGATE,
        STATE_EDIT
    };

    // Buffer
    static float    DSY_SDRAM_BSS buffer[MAX_BUFFER_SAMPLES];
    uint32_t        write_pos         = 0;
    uint32_t        buffer_len_samples = 48000;

    // Grains (now L/R)
    static Grain    grains_l[MAX_GRAINS];
    static Grain    grains_r[MAX_GRAINS];
    uint32_t        grain_trig_counter_l = 0;
    uint32_t        grain_trig_counter_r = 0;
    uint32_t        grain_trig_interval_l = 2400; 
    uint32_t        grain_trig_interval_r = 2400; 

    // State
    float           params[PARAM_COUNT];
    int             division_idx = 0; 
    const int       division_vals[4] = {1, 2, 4, 8}; 
    float           sample_rate_ = 48000.0f;
    Rand            rand_;
    
    // UI
    UiState         ui_state = STATE_NAVIGATE;
    int             selected_param = 0;
    int             view_top_param = 0; 
    bool            enc_click_pending = false;

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void GetSample(float &outl, float &outr, float inl, float inr);
    void UpdateBufferLen();
    void UpdateGrainParams();
};