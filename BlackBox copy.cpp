#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/disp/oled_display.h"
#include "dev/oled_ssd130x.h"
#include "util/oled_fonts.h"
// FIX: Use the Oscillator header instead of the LFO one.
// #include "dsp/oscillator.h"

// Use the daisy namespace
using namespace daisysp;
using namespace daisy;

// Declare a global instance of the Daisy Seed
static DaisySeed hw;

// Set max delay time to 2 seconds
#define MAX_DELAY static_cast<size_t>(48000 * 2.0f)

// Define max interval between taps for tempo detection (in ms)
#define MAX_TAP_INTERVAL 2000

// Delay lines for left and right channels, stored in SDRAM
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> delr;

// --- Component Declarations ---
static AnalogControl feedback_knob;
static AnalogControl mix_knob;
static Encoder       time_encoder;
static Encoder       aux_encoder;
static Switch        tap_switch;
static Switch        lfo_switch; // Name kept for clarity, but it toggles the oscillator
static Switch        clear_switch;
static Led           tempo_led;
// OLED display (SSD130x over I2C)
using OledDriver = daisy::SSD130xI2c128x64Driver;
static OledDisplay<OledDriver> display;

// --- OLED text helpers (180° rotation) ---
static void DrawCharRot180(OledDisplay<OledDriver> &disp,
                           int                      x,
                           int                      y,
                           char                     ch,
                           const FontDef &          font,
                           bool                     on)
{
    if(ch < 32 || ch > 126)
        return;
    for(int i = 0; i < (int)font.FontHeight; i++)
    {
        uint32_t rowBits = font.data[(ch - 32) * font.FontHeight + i];
        for(int j = 0; j < (int)font.FontWidth; j++)
        {
            bool bit_on = (rowBits << j) & 0x8000;
            int  rx     = (int)disp.Width() - 1 - (x + j);
            int  ry     = (int)disp.Height() - 1 - (y + i);
            if(rx >= 0 && ry >= 0 && rx < (int)disp.Width()
               && ry < (int)disp.Height())
            {
                disp.DrawPixel((uint_fast8_t)rx, (uint_fast8_t)ry, bit_on ? on : !on);
            }
        }
    }
}

static void DrawStringRot180(OledDisplay<OledDriver> &disp,
                             int                      x,
                             int                      y,
                             const char *             str,
                             const FontDef &          font,
                             bool                     on)
{
    int cx = x;
    while(*str)
    {
        DrawCharRot180(disp, cx, y, *str, font, on);
        cx += font.FontWidth;
        ++str;
    }
}
// MOD: Use an Oscillator object instead of an Lfo
static Oscillator    osc;


// Global variables for delay parameters
static float    current_delay, feedback, dry_wet_mix;
static float    time_as_float;
static float    sample_rate;
static uint32_t last_tap_time;
static bool     lfo_active  = false; // This flag now controls the oscillator
static bool     is_clearing = false;
static int32_t  aux_count   = 0;
static uint32_t screen_blink_start = 0;
static bool     screen_blink_active = false;


// Helper function to process inputs
void Controls();

// Helper function to get the processed delay sample
void GetDelaySample(float &outl, float &outr, float inl, float inr);


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    Controls();

    for(size_t i = 0; i < size; i++)
    {
        GetDelaySample(out[0][i], out[1][i], in[0][i], in[1][i]);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();

    dell.Init();
    delr.Init();

    AdcChannelConfig adcConfig[2];
    adcConfig[0].InitSingle(hw.GetPin(15));
    adcConfig[1].InitSingle(hw.GetPin(16));
    hw.adc.Init(adcConfig, 2);
    hw.adc.Start();

    feedback_knob.Init(hw.adc.GetPtr(0), sample_rate);
    mix_knob.Init(hw.adc.GetPtr(1), sample_rate);

    time_encoder.Init(hw.GetPin(5), hw.GetPin(6), hw.GetPin(4), hw.AudioCallbackRate());
    // Second encoder (test): channels on pins 7/8, click on 9
    aux_encoder.Init(hw.GetPin(8), hw.GetPin(7), hw.GetPin(9), hw.AudioCallbackRate());

    // Using pins 1, 2, and 3 for the switches
    lfo_switch.Init(hw.GetPin(1), hw.AudioCallbackRate());
    clear_switch.Init(hw.GetPin(2), hw.AudioCallbackRate());
    tap_switch.Init(hw.GetPin(3), hw.AudioCallbackRate());

    // MOD: Initialize the Oscillator
    osc.Init(sample_rate);
    osc.SetWaveform(Oscillator::WAVE_SIN);

    tempo_led.Init(hw.GetPin(21), false);

    // Initialize I2C OLED on pins 27 (SDA) and 28 (SCL)
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph
        = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.mode
        = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.speed
        = I2CHandle::Config::Speed::I2C_400KHZ;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda
        = hw.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl
        = hw.GetPin(11);
    disp_cfg.driver_config.transport_config.i2c_address = 0x3C;
    display.Init(disp_cfg);
    display.Fill(false);
    display.Update();

    time_as_float = sample_rate * 0.5f;
    fonepole(current_delay, time_as_float, 1.f);
    dell.SetDelay(current_delay);
    delr.SetDelay(current_delay);

    hw.StartAudio(AudioCallback);

    uint32_t last_blink_time = 0;
    bool     tempo_led_on    = false;
    while(1)
    {
        uint32_t now = System::GetNow();

        uint32_t delay_ms = static_cast<uint32_t>(current_delay / sample_rate * 1000.f);
        if(delay_ms > 0 && (now - last_blink_time > delay_ms))
        {
            last_blink_time = now;
            tempo_led.Set(1.0f);
            tempo_led_on = true;
        }
        if(tempo_led_on && (now - last_blink_time > 50))
        {
            tempo_led.Set(0.0f);
            tempo_led_on = false;
        }

        // OLED test: show pot values and current delay in ms
        // Blink screen white briefly on encoder button presses
        if(screen_blink_active && (now - screen_blink_start) < 120)
        {
            display.Fill(true);
            display.Update();
        }
        else
        {
            if(screen_blink_active)
                screen_blink_active = false;

            display.Fill(false);
            char line[32];
            // Title
            DrawStringRot180(display, 0, 0, "BlackBox", Font_7x10, true);
            // Mix (use integer percent to avoid float printf)
            int mix_pct = (int)(dry_wet_mix * 100.0f + 0.5f);
            snprintf(line, sizeof(line), "Mix: %3d%%", mix_pct);
            DrawStringRot180(display, 0, 14, line, Font_6x8, true);
            // Feedback (integer percent)
            int fbk_pct = (int)(feedback * 100.0f + 0.5f);
            snprintf(line, sizeof(line), "Fbk: %3d%%", fbk_pct);
            DrawStringRot180(display, 0, 24, line, Font_6x8, true);
            // Delay in ms
            uint32_t delay_ms_disp = static_cast<uint32_t>(current_delay / sample_rate * 1000.f);
            snprintf(line, sizeof(line), "Delay: %lu ms", (unsigned long)delay_ms_disp);
            DrawStringRot180(display, 0, 34, line, Font_6x8, true);
            // Aux encoder counter
            snprintf(line, sizeof(line), "Enc2: %ld", (long)aux_count);
            DrawStringRot180(display, 0, 44, line, Font_6x8, true);
            display.Update();
        }

        tempo_led.Update();
    }
}

void Controls()
{
    feedback    = feedback_knob.Process() * 0.98f;
    dry_wet_mix = mix_knob.Process();
    time_encoder.Debounce();
    aux_encoder.Debounce();
    tap_switch.Debounce();
    lfo_switch.Debounce();
    clear_switch.Debounce();

    if(lfo_switch.RisingEdge())
    {
        lfo_active = !lfo_active;
    }

    is_clearing = clear_switch.Pressed();
    if(clear_switch.RisingEdge())
    {
        dell.Reset();
        delr.Reset();
    }

    if(tap_switch.RisingEdge())
    {
        uint32_t now      = System::GetNow();
        uint32_t tap_diff = now - last_tap_time;

        if(tap_diff < MAX_TAP_INTERVAL)
        {
            time_as_float = static_cast<float>(tap_diff) * 0.001f * sample_rate;
        }
        last_tap_time = now;
    }

    int32_t encoder_inc = time_encoder.Increment();
    if(encoder_inc != 0)
    {
        float scale_factor = 500.0f;
        time_as_float += encoder_inc * scale_factor;
    }

    // Aux encoder increments counter
    int32_t aux_inc = aux_encoder.Increment();
    if(aux_inc != 0)
    {
        aux_count += aux_inc;
    }

    // Blink request on either encoder press
    if(time_encoder.RisingEdge() || aux_encoder.RisingEdge())
    {
        screen_blink_active = true;
        screen_blink_start  = System::GetNow();
    }

    float min_delay = 100.0f;
    time_as_float   = fclamp(time_as_float, min_delay, MAX_DELAY - 4.f);

    if(time_as_float > 0.f)
    {
        // MOD: Set Oscillator frequency
        float osc_freq = (sample_rate / time_as_float) * 4.0f;
        osc.SetFreq(osc_freq);
    }
}


void GetDelaySample(float &outl, float &outr, float inl, float inr)
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
        // MOD: Process the Oscillator and map its output
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