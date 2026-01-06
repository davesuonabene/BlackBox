// Glue main: Bare Bones Version
#include "config.h"
#include "hw.h"
#include "screen.h"
#include "processing.h"

using namespace daisy;
using namespace daisysp;

static Hardware   g_hw;
static Screen     g_screen;
static Processing g_proc;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    g_proc.Controls(g_hw);

    for(size_t i = 0; i < size; i++)
    {
        g_proc.ProcessAudio(out[0][i], out[1][i], in[0][i], in[1][i]);
    }
}

int main(void)
{
    g_hw.Init();
    g_screen.Init(g_hw.seed);
    g_proc.Init(g_hw);

    g_hw.seed.StartAudio(AudioCallback);

    uint32_t last_ui = 0;
    const uint32_t ui_interval_ms = 33; // ~30 Hz

    while(1)
    {
        uint32_t now = System::GetNow();

        // Handle Encoder Clicks (Blink Trigger)
        if(g_proc.enc_click_pending)
        {
            g_proc.enc_click_pending = false;
            g_screen.Blink(now);
        }

        // Update UI
        if(now - last_ui >= ui_interval_ms)
        {
            last_ui = now;
            g_screen.DrawStatus(g_proc);
        }
    }
}