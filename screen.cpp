#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
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

// --- HELPER: Normalize Parameter Value ---
static float GetNormVal(int param_id, float val, int division_idx = 0)
{
    float norm = 0.0f;
    switch(param_id)
    {
        case PARAM_PRE_GAIN:
        case PARAM_POST_GAIN:
        case PARAM_SEND:
        case PARAM_FEEDBACK:
        case PARAM_MIX:
        case PARAM_STEREO:
            norm = val; 
            break;
        case PARAM_BPM:
            norm = (val - 20.f) / (300.f - 20.f); 
            break;
        case PARAM_DIVISION:
            norm = (float)division_idx / 3.0f; 
            break;
        case PARAM_PITCH:
        {
            float semitones = 12.0f * log2f(val);
            norm = (semitones + 24.f) / 48.f; 
            break;
        }
        case PARAM_GRAIN_SIZE:
            norm = (val - 0.002f) / (0.5f - 0.002f); 
            break;
        case PARAM_GRAIN_DENSITY:
            norm = (val - 0.5f) / (50.f - 0.5f); 
            break;
    }
    if(norm < 0.0f) norm = 0.0f;
    if(norm > 1.0f) norm = 1.0f;
    return norm;
}


// --- STYLING FUNCTIONS ---

static void DrawValueBar(int y, float norm_base, float norm_eff, const char* text, bool selected)
{
    const FontDef& font = Font_7x10;
    int bar_y = y - 1;
    int bar_height = font.FontHeight + 2;
    
    int w_base = (int)(norm_base * (float)kRightColWidth);
    int w_eff  = (int)(norm_eff  * (float)kRightColWidth);
    
    if(w_base > kRightColWidth) w_base = kRightColWidth;
    if(w_eff > kRightColWidth)  w_eff  = kRightColWidth;

    int w_min = (w_base < w_eff) ? w_base : w_eff;
    int w_max = (w_base > w_eff) ? w_base : w_eff;
    int gap = 2; 

    // Helper to draw solid segment
    auto DrawSolidSegment = [&](int start_w, int end_w) {
        if (end_w <= start_w) return;
        int rx_start = display.Width() - 1 - (kRightColX + end_w - 1);
        int rx_end   = display.Width() - 1 - (kRightColX + start_w);
        int ry_start = display.Height() - 1 - (bar_y + bar_height - 1);
        int ry_end   = display.Height() - 1 - (bar_y);
        display.DrawRect(rx_start, ry_start, rx_end, ry_end, true, true);
    };

    // 1. Draw "Safe" part (Solid)
    DrawSolidSegment(0, w_min);

    // 2. Draw "Modulated" part (Hatched)
    if (w_max > w_min + gap)
    {
        int ry_start = display.Height() - 1 - (bar_y + bar_height - 1);
        int ry_end   = display.Height() - 1 - (bar_y);
        
        for (int i = w_min + gap; i < w_max; i++)
        {
            if (i % 3 == 0) 
            {
                int rx = display.Width() - 1 - (kRightColX + i);
                display.DrawLine(rx, ry_start, rx, ry_end, true);
            }
        }
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

void Screen::Blink(uint32_t now)
{
    blink_active = true;
    blink_start = now;
}

void Screen::DrawStatus(Processing &proc)
{
    if (proc.trigger_blink)
    {
        Blink(daisy::System::GetNow());
        proc.trigger_blink = false; 
    }

    display.Fill(false);

    if (blink_active)
    {
        if (daisy::System::GetNow() - blink_start < 100) 
        {
            display.Fill(true); 
            display.Update();
            return; 
        }
        else
        {
            blink_active = false;
        }
    }

    char label_str[32];
    char value_str[16];

    // Determine Layout
    bool is_main_menu = (proc.current_menu == kMenuMain);
    
    // Header for Secondary Menu
    if (!is_main_menu)
    {
        // Draw Header Title at Y=0
        DrawStringRot180(display, 0, 0, proc.parent_menu_name, Font_7x10, true);
        // Draw Header Separator at Y=10
        int line_ry = display.Height() - 1 - 10;
        display.DrawLine(0, line_ry, 127, line_ry, true);
    }
    
    // List start Y
    // Main: 0 (or centered?). Let's keep existing logic -> starts at 0.
    // Secondary: 12 (below header line)
    int y_list_start = is_main_menu ? 0 : 12;
    int max_lines = 4;

    for(int line_idx = 0; line_idx < max_lines; line_idx++)
    {
        int item_idx = proc.view_top_item_idx + line_idx;

        if(item_idx >= proc.current_menu_size)
            break; 

        const MenuItem& item = proc.current_menu[item_idx];
        
        bool is_selected = (item_idx == proc.selected_item_idx);
        bool is_editing = (is_selected && proc.ui_state == proc.STATE_PARAM_EDIT);

        // --- Y Position Calculation ---
        int y_pos = y_list_start + line_idx * 10;
        
        // Add extra gap after "Back" button (Index 1) in secondary menus
        // If the item we are drawing is after index 1, shift it down.
        if (!is_main_menu && item_idx > 1) 
        {
            y_pos += 5; 
        }

        snprintf(label_str, sizeof(label_str), "%s", item.name);
        value_str[0] = '\0';
        float norm_base = 0.0f;
        float norm_eff  = 0.0f;

        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU)
        {
            if (item.param_id == PARAM_MAP_AMT)
            {
                float map_val = proc.knob_map_amounts[proc.edit_param_target];
                snprintf(value_str, sizeof(value_str), "%d%%", (int)(map_val * 100.0f));
                norm_base = (map_val + 1.0f) * 0.5f; 
                norm_eff = norm_base; 
            }
            else
            {
                float val_base = proc.params[item.param_id];
                float val_eff  = proc.effective_params[item.param_id];

                switch(item.param_id)
                {
                    case PARAM_PRE_GAIN:
                    case PARAM_POST_GAIN:
                        snprintf(value_str, sizeof(value_str), "%d%%", (int)(val_base * 200.f));
                        break;
                    case PARAM_SEND:
                    case PARAM_FEEDBACK:
                    case PARAM_MIX:
                    case PARAM_STEREO:
                        snprintf(value_str, sizeof(value_str), "%d%%", (int)(val_base * 100.f));
                        break;
                    case PARAM_BPM:
                        snprintf(value_str, sizeof(value_str), "%d", (int)val_base);
                        break;
                    case PARAM_DIVISION:
                        snprintf(value_str, sizeof(value_str), "1/%d", (int)val_base);
                        break;
                    case PARAM_PITCH:
                    {
                        float semitones = 12.0f * log2f(val_base);
                        int int_part    = (int)semitones;
                        int frac_part   = (int)(fabsf(semitones * 10.0f)) % 10;
                        snprintf(value_str, sizeof(value_str), "%+d.%dst", int_part, frac_part);
                        break;
                    }
                    case PARAM_GRAIN_SIZE:
                        snprintf(value_str, sizeof(value_str), "%dms", (int)(val_base * 1000.f));
                        break;
                    case PARAM_GRAIN_DENSITY:
                        snprintf(value_str, sizeof(value_str), "%dHz", (int)val_base);
                        break;
                }

                norm_base = GetNormVal(item.param_id, val_base, proc.division_idx);
                norm_eff  = GetNormVal(item.param_id, val_eff,  proc.division_idx);
            }
        }

        // Draw Value Bar
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU)
        {
            DrawValueBar(y_pos, norm_base, norm_eff, value_str, is_selected);
        }
        
        // Draw Label
        int label_width = GetStringWidth(label_str, Font_7x10);
        int label_x = (kLeftColX + kLeftColWidth) - label_width - 2; 
        if (label_x < kLeftColX) label_x = kLeftColX; 

        if(is_selected)
        {
            int box_h = Font_7x10.FontHeight + 2;
            DrawHighlightBox(display, kLeftColX, y_pos - 1, kLeftColWidth, box_h, true);
        }
        
        DrawStringRot180(display, label_x, y_pos, label_str, Font_7x10, !is_editing);
        
        // --- DRAW LIST SEPARATOR ---
        // If this is the "Back" button (index 1) in a secondary menu, draw line below it
        if (!is_main_menu && item_idx == 1)
        {
            // The line is in the middle of the 5px gap
            // y_pos of Back button is around 22.
            // Next item starts at y_pos + 10 + 5.
            // Line should be at y_pos + 10 + 2.
            int line_y_screen = y_pos + 12;
            int ry = display.Height() - 1 - line_y_screen;
            display.DrawLine(0, ry, 127, ry, true);
        }
    }
    
    // --- Footer ---
    // Only show "alpha version" on Main Menu.
    // Secondary menus have the Title Header at top.
    if (is_main_menu)
    {
        DrawStringRot180(display, 0, 53, "alpha version", Font_7x10, true);
    }

    display.Update();
}