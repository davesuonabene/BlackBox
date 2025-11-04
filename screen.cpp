#include "screen.h"
#include <cstdio>
#include <string.h> // For strncat
#include <math.h>   // For log2f, fabsf
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

// --- CONSTANTS FOR LAYOUT ---
// Screen is 128px wide.
// Right column (Value) is 40% = ~51px
// Left column (Label) is 60% = ~77px
//
// Rotated Coordinates:
// x=0 is physical RIGHT. x=127 is physical LEFT.

// Right Column (Value): 40% (51px), on the physical right.
const int kRightColX = 0;
const int kRightColWidth = 51; 

// Left Column (Label): 60% (77px), on the physical left.
const int kLeftColX = 51; // Starts after the right column
const int kLeftColWidth = 77;


// --- LOW-LEVEL DRAWING HELPERS ---

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

static void DrawHighlightBox(OledDisplay<OledDriver> &disp,
                             int x, int y, int w, int h, bool on)
{
    int rx = disp.Width() - 1 - (x + w - 1);
    int ry = disp.Height() - 1 - (y + h - 1);
    // Use DrawRect with fill=false for a hollow box
    disp.DrawRect(rx, ry, rx + w - 1, ry + h - 1, on, false);
}

/** Calculates the pixel width of a string */
static int GetStringWidth(const char *str, const FontDef &font)
{
    int w = 0;
    while(*str)
    {
        w += font.FontWidth;
        ++str;
    }
    return w;
}

// --- NEW STYLING FUNCTIONS ---

/** Draws the value bar and inverted text in the right column */
static void DrawValueBar(int y, float norm_val, const char* text, bool selected)
{
    const FontDef& font = Font_7x10;
    int bar_y = y - 1;
    int bar_height = font.FontHeight + 2;

    // Calculate bar width from normalized value
    int bar_width = (int)(norm_val * (float)kRightColWidth);
    if(bar_width < 0) bar_width = 0;
    if(bar_width > kRightColWidth) bar_width = kRightColWidth;

    // 1. Draw the filled part of the bar (starts at x=0)
    int rx_fill = display.Width() - 1 - (kRightColX + bar_width - 1);
    int ry_fill = display.Height() - 1 - (bar_y + bar_height - 1);
    display.DrawRect(rx_fill, ry_fill, rx_fill + bar_width - 1, ry_fill + bar_height - 1, true, true);

    // 2. Draw the hollow part (background)
    if (bar_width < kRightColWidth)
    {
        int rx_empty = display.Width() - 1 - (kRightColX + kRightColWidth - 1);
        int ry_empty = display.Height() - 1 - (bar_y + bar_height - 1);
        display.DrawRect(rx_empty, ry_empty, rx_fill, ry_empty + bar_height - 1, true, false);
    }
    
    // 3. Draw the text, always inverted (color=false)
    DrawStringRot180(display, kRightColX + 2, y, text, font, false);
}

// --- SCREEN CLASS FUNCTIONS ---

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
    char label_str[32];
    char value_str[16];

    const int y_top = 10;
    const int y_spacing = 10;
    const int max_lines = 5; 

    for(int line_idx = 0; line_idx < max_lines; line_idx++)
    {
        int param_idx = proc.view_top_param + line_idx;

        if(param_idx >= proc.PARAM_COUNT)
            break; 

        const char *name = proc.param_names[param_idx];
        float       val  = proc.params[param_idx];
        
        bool is_selected = (param_idx == proc.selected_param);
        bool is_editing = (is_selected && proc.ui_state == proc.STATE_EDIT);

        // Build the label string
        snprintf(label_str, sizeof(label_str), "%s:", name);
        
        // Build the value string and get normalized value (0.0 - 1.0)
        float norm_val = val; // Default for 0-1 params
        switch(param_idx)
        {
            case proc.PARAM_PRE_GAIN:
            case proc.PARAM_POST_GAIN:
                snprintf(value_str, sizeof(value_str), "%d%%", (int)(val * 200.f));
                norm_val = val; // Already 0-1
                break;
            case proc.PARAM_SEND:
            case proc.PARAM_FEEDBACK:
            case proc.PARAM_MIX:
            case proc.PARAM_STEREO:
                snprintf(value_str, sizeof(value_str), "%d%%", (int)(val * 100.f));
                norm_val = val; // Already 0-1
                break;
            case proc.PARAM_BPM:
                snprintf(value_str, sizeof(value_str), "%d", (int)val);
                norm_val = (val - 20.f) / (300.f - 20.f); // Normalize 20-300
                break;
            case proc.PARAM_DIVISION:
                snprintf(value_str, sizeof(value_str), "1/%d", (int)val);
                norm_val = (float)proc.division_idx / 3.0f; // Normalize 0-3
                break;
            case proc.PARAM_PITCH:
            {
                float semitones = 12.0f * log2f(val);
                int int_part    = (int)semitones;
                int frac_part   = (int)(fabsf(semitones * 10.0f)) % 10;
                snprintf(value_str, sizeof(value_str), "%+d.%dst", int_part, frac_part);
                norm_val = (semitones + 24.f) / 48.f; // Normalize -24 to +24
                break;
            }
            case proc.PARAM_GRAIN_SIZE:
                snprintf(value_str, sizeof(value_str), "%dms", (int)(val * 1000.f));
                norm_val = (val - 0.002f) / (0.5f - 0.002f); // Normalize 2ms-500ms
                break;
            case proc.PARAM_GRAIN_DENSITY:
                snprintf(value_str, sizeof(value_str), "%dHz", (int)val);
                norm_val = (val - 0.5f) / (50.f - 0.5f); // Normalize 0.5-50Hz
                break;
        }

        int y_pos = y_top + line_idx * y_spacing;
        
        // --- Draw Right Column (Value) ---
        // This is on the physical right (x=0 to 51)
        DrawValueBar(y_pos, norm_val, value_str, is_selected);

        // --- Draw Left Column (Label) ---
        // This is on the physical left (x=51 to 127)
        
        // Justification: Left-aligned (physical left)
        // We draw from the far left (x=127) minus width
        int label_width = GetStringWidth(label_str, Font_7x10);
        int label_x = (kLeftColX + kLeftColWidth) - label_width - 2; // -2 for padding
        if (label_x < kLeftColX) label_x = kLeftColX; // Clamp

        // Highlight logic: hollow box on left column
        if(is_selected)
        {
            int box_h = Font_7x10.FontHeight + 2;
            DrawHighlightBox(display, kLeftColX, y_pos - 1, kLeftColWidth, box_h, true);
        }
        
        // Invert label text if editing, draw at calculated 'x'
        DrawStringRot180(display, label_x, y_pos, label_str, Font_7x10, !is_editing);
    }
    
    display.Update();
}