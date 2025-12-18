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
        float in_l = in[0][i];
        float in_r = in[1][i];

        // Sum active loop to input before processing for resampling
        bool should_play = (g_hw.looper_mode == Hardware::LP_PLAYING) || 
                           (g_hw.looper_mode == Hardware::LP_RECORDING && g_hw.loop_length > 0);

        if (should_play && g_hw.active_buffer != nullptr)
        {
            if (g_hw.play_pos < g_hw.loop_length)
            {
                in_l += g_hw.active_buffer[g_hw.play_pos * 2];
                in_r += g_hw.active_buffer[g_hw.play_pos * 2 + 1];
            }
            g_hw.play_pos++;
            if (g_hw.play_pos >= g_hw.loop_length) g_hw.play_pos = 0;
        }

        g_proc.GetSample(out[0][i], out[1][i], in_l, in_r);

        // Record output to rec_buffer (New Loop)
        if (g_hw.looper_mode == Hardware::LP_RECORDING && g_hw.rec_buffer != nullptr)
        {
            if (g_hw.rec_pos < (LOOPER_MAX_SAMPLES / 2))
            {
                g_hw.rec_buffer[g_hw.rec_pos * 2]     = out[0][i];
                g_hw.rec_buffer[g_hw.rec_pos * 2 + 1] = out[1][i];
                g_hw.rec_pos++;
            }
            else
            {
                g_hw.SwitchToNewLoop();
                g_hw.looper_mode = Hardware::LP_PLAYING;
            }
        }
    }
}

int main(void)
{
    g_hw.Init();
    g_screen.Init(g_hw.seed);
    g_proc.Init(g_hw);

    g_hw.seed.StartAudio(AudioCallback);

    uint32_t last_ui_update = 0;
    while(1)
    {
        uint32_t now = System::GetNow();
        if(now - last_ui_update >= 33) 
        {
            last_ui_update = now;
            g_screen.DrawStatus(g_proc, g_hw);
        }
    }
}