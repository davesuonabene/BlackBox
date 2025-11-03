// Glue main: use modules
#include "config.h"
#include "hw.h"
#include "screen.h"
#include "processing.h"

using namespace daisy;
using namespace daisysp;

static Hardware   g_hw;
static Screen     g_screen;
static Processing g_proc;


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    g_proc.Controls(g_hw);
    for(size_t i = 0; i < size; i++)
    {
        g_proc.Process(out[0][i], out[1][i], in[0][i], in[1][i]);
    }
}

int main(void)
{
    g_hw.Init();
    g_screen.Init(g_hw.seed);
    g_proc.Init(g_hw);

    g_hw.seed.StartAudio(AudioCallback);

    uint32_t last_ui = 0;
    const uint32_t ui_interval_ms = 50; // ~20 Hz minimal UI
    while(1)
    {
        uint32_t now = System::GetNow();
        if(now - last_ui >= ui_interval_ms)
        {
            last_ui = now;
            g_screen.DrawStatus(g_proc.frequency, g_proc.muted);
        }
    }
}


/*
// Glue main: use modules
#include "config.h"
#include "hw.h"
#include "screen.h"
#include "processing.h"

using namespace daisy;
using namespace daisysp;

static Hardware   g_hw;
static Screen     g_screen;
static Processing g_proc;


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    g_proc.Controls(g_hw);

    for(size_t i = 0; i < size; i++)
    {
        g_proc.GetDelaySample(out[0][i], out[1][i], in[0][i], in[1][i], g_hw.sample_rate);
    }
}

int main(void)
{
    g_hw.Init();
    g_screen.Init(g_hw.seed);
    g_proc.Init(g_hw);

    g_hw.seed.StartAudio(AudioCallback);

    uint32_t last_blink_time = 0;
    bool     tempo_led_on    = false;
    uint32_t last_ui = 0;
    const uint32_t ui_interval_ms = 33; // ~30 Hz
    while(1)
    {
        uint32_t now = System::GetNow();

        // LED blinks at BPM rate, not division rate
        uint32_t bpm_delay_ms = static_cast<uint32_t>(60000.0f / g_proc.bpm);
        if(bpm_delay_ms > 0 && (now - last_blink_time > bpm_delay_ms))
        {
            last_blink_time = now;
            g_hw.tempo_led.Set(1.0f);
            tempo_led_on = true;
        }
        if(tempo_led_on && (now - last_blink_time > 50))
        {
            g_hw.tempo_led.Set(0.0f);
            tempo_led_on = false;
        }

        if(g_proc.enc_click_pending)
        {
            g_proc.enc_click_pending = false;
            g_screen.Blink(now);
        }

        if(now - last_ui >= ui_interval_ms)
        {
            last_ui = now;
            int mix_pct = (int)(g_proc.dry_wet_mix * 100.0f + 0.5f);
            int fbk_pct = (int)(g_proc.feedback * 100.0f + 0.5f);
            g_screen.DrawStatus(mix_pct,
                                fbk_pct,
                                static_cast<uint32_t>(g_proc.current_delay / g_hw.sample_rate * 1000.f),
                                static_cast<int>(g_proc.bpm + 0.5f),
                                g_proc.division,
                                g_proc.selected_param,
                                g_proc.master_of_param[g_proc.PARAM_MIX],
                                g_proc.master_of_param[g_proc.PARAM_FEEDBACK],
                                g_proc.master_of_param[g_proc.PARAM_BPM],
                                g_proc.master_of_param[g_proc.PARAM_DIVISION],
                                true);
        }

        g_hw.tempo_led.Update();
    }
}
*/