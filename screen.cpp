#include "screen.h"
#include <cstdio>
#include <string.h> // For strncat
#include <math.h>   // For log2f, fabsf
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

// Helper function from original file to draw rotated char
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

// Helper function from original file to draw rotated string
static void DrawStringRot180(OledDisplay<OledDriver> &disp,
                             int                      x,
                             int                      y,
                             const char * str,
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


void Screen::Init(DaisySeed &seed)
{
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph
        = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.mode
        = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.speed
        = I2CHandle::Config::Speed::I2C_1MHZ;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda
        = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl
        = seed.GetPin(11);
    disp_cfg.driver_config.transport_config.i2c_address = 0x3C;
    display.Init(disp_cfg);
    
    display.Fill(false);
    display.Update();
}

void Screen::Blink(uint32_t now)
{
    blink_active = true;
    blink_start  = now;
}

void Screen::DrawStatus(Processing &proc)
{
    if(blink_active && (System::GetNow() - blink_start) < 100)
    {
        display.Fill(true);
        display.Update();
        return;
    }
    if(blink_active)
        blink_active = false;

    display.Fill(false);
    char line[32];

    const int y_top = 10;
    const int y_spacing = 10;
    const int x_left = 0;
    const int max_lines = 5; 

    for(int line_idx = 0; line_idx < max_lines; line_idx++)
    {
        int param_idx = proc.view_top_param + line_idx;

        if(param_idx >= proc.PARAM_COUNT)
            break; 

        const char *name = proc.param_names[param_idx];
        float       val  = proc.params[param_idx];
        
        const char *prefix = "  ";
        if(param_idx == proc.selected_param)
        {
            prefix = (proc.ui_state == proc.STATE_EDIT) ? "[ " : "> ";
        }
        
        line[0] = '\0'; 

        switch(param_idx)
        {
            // 0-200% params (stored 0-1)
            case proc.PARAM_PRE_GAIN:
            case proc.PARAM_POST_GAIN:
                snprintf(line, sizeof(line), "%s%s: %d%%", prefix, name, (int)(val * 200.f));
                break;
            // 0-100% params (stored 0-1)
            case proc.PARAM_SEND:
            case proc.PARAM_FEEDBACK:
            case proc.PARAM_MIX:
            case proc.PARAM_STEREO:
                snprintf(line, sizeof(line), "%s%s: %d%%", prefix, name, (int)(val * 100.f));
                break;
            case proc.PARAM_BPM:
                snprintf(line, sizeof(line), "%s%s: %d", prefix, name, (int)val);
                break;
            case proc.PARAM_DIVISION:
                snprintf(line, sizeof(line), "%s%s: 1/%d", prefix, name, (int)val);
                break;
            case proc.PARAM_PITCH:
            {
                float semitones = 12.0f * log2f(val);
                int int_part    = (int)semitones;
                int frac_part   = (int)(fabsf(semitones * 10.0f)) % 10;
                snprintf(line, sizeof(line), "%s%s: %+d.%dst", prefix, name, int_part, frac_part);
                break;
            }
            case proc.PARAM_GRAIN_SIZE:
                snprintf(line, sizeof(line), "%s%s: %dms", prefix, name, (int)(val * 1000.f));
                break;
            case proc.PARAM_GRAIN_DENSITY:
                snprintf(line, sizeof(line), "%s%s: %dHz", prefix, name, (int)val);
                break;
        }

        if(param_idx == proc.selected_param && proc.ui_state == proc.STATE_EDIT)
        {
            strncat(line, " ]", sizeof(line) - strlen(line) - 1);
        }

        int y_pos = y_top + line_idx * y_spacing;
        DrawStringRot180(display, x_left, y_pos, line, Font_7x10, true);
    }
    
    display.Update();
}