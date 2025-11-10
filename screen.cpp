#include "screen.h"
#include <cstdio>
#include <string.h> // For strncat
#include <math.h>   // For log2f, fabsf
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

// --- CONSTANTS FOR LAYOUT ---
const int kRightColX = 0;
const int kRightColWidth = 51; 
const int kLeftColX = 51;
const int kLeftColWidth = 77;

// --- LOW-LEVEL DRAWING HELPERS ---
// ... (DrawCharRot180, DrawStringRot180, DrawHighlightBox, GetStringWidth) ...
static void DrawCharRot180(OledDisplay<OledDriver> &disp,
                           int                      x,
                           int                      y,
                           char                     ch,
                           const FontDef &          font,
                           bool                     on)
{
    if(ch < 32 || ch > 126) return;
    for(int i = 0; i < (int)font.FontHeight; i++)
    {
        uint32_t rowBits = font.data[(ch - 32) * font.FontHeight + i];
        for(int j = 0; j < (int)font.FontWidth; j++)
        {
            bool bit_on = (rowBits << j) & 0x8000;
            int  rx     = (int)disp.Width() - 1 - (x + j);
            int  ry     = (int)disp.Height() - 1 - (y + i);
            if(rx >= 0 && ry >= 0 && rx < (int)disp.Width() && ry < (int)disp.Height())
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
    disp.DrawRect(rx, ry, rx + w - 1, ry + h - 1, on, false);
}

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


// --- STYLING FUNCTIONS ---

static void DrawValueBar(int y, float norm_val, const char* text, bool selected)
{
    const FontDef& font = Font_7x10;
    int bar_y = y - 1;
    int bar_height = font.FontHeight + 2;
    int bar_width = (int)(norm_val * (float)kRightColWidth);
    if(bar_width < 0) bar_width = 0;
    if(bar_width > kRightColWidth) bar_width = kRightColWidth;

    int rx_fill = display.Width() - 1 - (kRightColX + bar_width - 1);
    int ry_fill = display.Height() - 1 - (bar_y + bar_height - 1);
    display.DrawRect(rx_fill, ry_fill, rx_fill + bar_width - 1, ry_fill + bar_height - 1, true, true);

    if (bar_width < kRightColWidth)
    {
        int rx_empty = display.Width() - 1 - (kRightColX + kRightColWidth - 1);
        int ry_empty = display.Height() - 1 - (bar_y + bar_height - 1);
        display.DrawRect(rx_empty, ry_empty, rx_fill, ry_empty + bar_height - 1, true, false);
    }
    
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

void Screen::DrawStatus(Processing &proc)
{
    display.Fill(false);
    char label_str[32];
    char value_str[16];

    const int y_top = 10;
    const int y_spacing = 10;
    const int max_lines = 5; 

    for(int line_idx = 0; line_idx < max_lines; line_idx++)
    {
        int item_idx = proc.view_top_item_idx + line_idx;

        if(item_idx >= proc.current_menu_size)
            break; 

        const MenuItem& item = proc.current_menu[item_idx];
        
        bool is_selected = (item_idx == proc.selected_item_idx);
        bool is_editing = (is_selected && proc.ui_state == proc.STATE_PARAM_EDIT);

        snprintf(label_str, sizeof(label_str), "%s", item.name);
        
        value_str[0] = '\0';
        float norm_val = 0.0f;

        // --- UPDATE: Check for both PARAM and PARAM_SUBMENU ---
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU)
        {
            float val = proc.params[item.param_id];
            
            switch(item.param_id)
            {
                case PARAM_PRE_GAIN:
                case PARAM_POST_GAIN:
                    snprintf(value_str, sizeof(value_str), "%d%%", (int)(val * 200.f));
                    norm_val = val; 
                    break;
                case PARAM_SEND:
                case PARAM_FEEDBACK:
                case PARAM_MIX:
                case PARAM_STEREO:
                    snprintf(value_str, sizeof(value_str), "%d%%", (int)(val * 100.f));
                    norm_val = val; 
                    break;
                case PARAM_BPM:
                    snprintf(value_str, sizeof(value_str), "%d", (int)val);
                    norm_val = (val - 20.f) / (300.f - 20.f); 
                    break;
                case PARAM_DIVISION:
                    snprintf(value_str, sizeof(value_str), "1/%d", (int)val);
                    norm_val = (float)proc.division_idx / 3.0f; 
                    break;
                case PARAM_PITCH:
                {
                    float semitones = 12.0f * log2f(val);
                    int int_part    = (int)semitones;
                    int frac_part   = (int)(fabsf(semitones * 10.0f)) % 10;
                    snprintf(value_str, sizeof(value_str), "%+d.%dst", int_part, frac_part);
                    norm_val = (semitones + 24.f) / 48.f; 
                    break;
                }
                case PARAM_GRAIN_SIZE:
                    snprintf(value_str, sizeof(value_str), "%dms", (int)(val * 1000.f));
                    norm_val = (val - 0.002f) / (0.5f - 0.002f); 
                    break;
                case PARAM_GRAIN_DENSITY:
                    snprintf(value_str, sizeof(value_str), "%dHz", (int)val);
                    norm_val = (val - 0.5f) / (50.f - 0.5f); 
                    break;
            }
        }

        int y_pos = y_top + line_idx * y_spacing;
        
        // --- Draw Right Column (Value) ---
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU)
        {
            DrawValueBar(y_pos, norm_val, value_str, is_selected);
        }
        
        // --- Draw Left Column (Label) ---
        int label_width = GetStringWidth(label_str, Font_7x10);
        int label_x = (kLeftColX + kLeftColWidth) - label_width - 2; 
        if (label_x < kLeftColX) label_x = kLeftColX; 

        if(is_selected)
        {
            int box_h = Font_7x10.FontHeight + 2;
            DrawHighlightBox(display, kLeftColX, y_pos - 1, kLeftColWidth, box_h, true);
        }
        
        DrawStringRot180(display, label_x, y_pos, label_str, Font_7x10, !is_editing);
    }
    
    display.Update();
}