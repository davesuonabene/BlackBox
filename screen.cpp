#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

// --- CONSTANTS FOR LAYOUT ---
// Total screen width: 128px
// Selector: 3px | Name: ~57px | Bar: 64px (50%)
const int kSelectorColX = 0;
const int kTextColX     = 5;
const int kTextColWidth = 55; 
const int kBarColX      = 64; 
const int kBarColWidth  = 64; // Exactly 50% of screen width

// --- LOW-LEVEL DRAWING HELPERS ---
static void DrawCharRot180(OledDisplay<OledDriver> &disp, int x, int y, char ch, const FontDef &font, bool on)
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
                disp.DrawPixel((uint_fast8_t)rx, (uint_fast8_t)ry, bit_on ? on : !on);
        }
    }
}

static void DrawStringRot180(OledDisplay<OledDriver> &disp, int x, int y, const char * str, const FontDef &font, bool on)
{
    int cx = x;
    while(*str) { DrawCharRot180(disp, cx, y, *str, font, on); cx += font.FontWidth; ++str; }
}

static void DrawSelectionIndicator(int y, bool engaged)
{
    int rx = display.Width() - 1 - kSelectorColX;
    int ry_start = display.Height() - 1 - (y + 9);
    int ry_end = display.Height() - 1 - y;
    
    // Slim | selector
    display.DrawLine(rx, ry_start, rx, ry_end, true);
    if (engaged) display.DrawLine(rx + 2, ry_start, rx + 2, ry_end, true);
}

static void DrawHighlightBox(OledDisplay<OledDriver> &disp, int x, int y, int_fast16_t w, int_fast16_t h, bool on)
{
    int rx = disp.Width() - 1 - (x + w - 1);
    int ry = disp.Height() - 1 - (y + h - 1);
    disp.DrawRect(rx, ry, rx + w - 1, ry + h - 1, on, true);
}

static float GetNormVal(int param_id, float val, int division_idx)
{
    float norm = 0.0f;
    switch(param_id) {
        case PARAM_PRE_GAIN:
        case PARAM_POST_GAIN:
        case PARAM_SEND:
        case PARAM_FEEDBACK:
        case PARAM_MIX:
        case PARAM_STEREO: norm = val; break;
        case PARAM_BPM: norm = (val - 20.f) / (300.f - 20.f); break;
        case PARAM_DIVISION: norm = (float)division_idx / 3.0f; break;
        case PARAM_PITCH: {
            float st = 12.0f * log2f(val);
            norm = (st + 24.f) / 48.f;
            break;
        }
        case PARAM_GRAIN_SIZE: norm = (val - 0.002f) / (0.5f - 0.002f); break;
        case PARAM_GRAIN_DENSITY: norm = (val - 0.5f) / (50.f - 0.5f); break;
    }
    return (norm < 0.0f) ? 0.0f : (norm > 1.0f ? 1.0f : norm);
}

static void DrawValueBar(int y, float norm_base, float norm_eff)
{
    int bar_height = 8; 
    int w_base = (int)(norm_base * (float)kBarColWidth);
    int w_eff  = (int)(norm_eff  * (float)kBarColWidth);
    int w_min = (w_base < w_eff) ? w_base : w_eff;
    int w_max = (w_base > w_eff) ? w_base : w_eff;

    auto DrawSegment = [&](int start_w, int end_w, bool solid) {
        if (end_w <= start_w) return;
        int rx_s = display.Width() - 1 - (kBarColX + end_w - 1);
        int rx_e = display.Width() - 1 - (kBarColX + start_w);
        int ry_s = display.Height() - 1 - (y + bar_height - 1);
        int ry_e = display.Height() - 1 - y;
        if(solid) display.DrawRect(rx_s, ry_s, rx_e, ry_e, true, true);
        else for(int i=start_w; i<end_w; i++) if(i%3==0) display.DrawLine(display.Width()-1-(kBarColX+i), ry_s, display.Width()-1-(kBarColX+i), ry_e, true);
    };

    DrawSegment(0, w_min, true);
    DrawSegment(w_min + 2, w_max, false);
}

void Screen::Init(DaisySeed &seed)
{
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed.GetPin(11);
    display.Init(disp_cfg);
}

void Screen::Blink(uint32_t now) { blink_active = true; blink_start = now; }

void Screen::DrawStatus(Processing &proc)
{
    if (proc.trigger_blink) { Blink(System::GetNow()); proc.trigger_blink = false; }
    display.Fill(false);
    if (blink_active && (System::GetNow() - blink_start < 100)) { display.Fill(true); display.Update(); return; }

    bool is_main = (proc.current_menu == kMenuMain);
    if (!is_main) {
        DrawStringRot180(display, 0, 0, proc.parent_menu_name, Font_7x10, true);
        display.DrawLine(0, display.Height()-11, 127, display.Height()-11, true);
    }

    int y_start = is_main ? 0 : 12;
    for(int i = 0; i < 4; i++) {
        int idx = proc.view_top_item_idx + i;
        if(idx >= proc.current_menu_size) break;

        const MenuItem& item = proc.current_menu[idx];
        bool sel = (idx == proc.selected_item_idx);
        bool edit = (sel && proc.ui_state == proc.STATE_PARAM_EDIT);
        int y = y_start + i * 11 + (!is_main && idx > 1 ? 5 : 0);

        char value_str[16] = "";
        char label_str[32] = "";
        float n_base = 0.0f, n_eff = 0.0f;

        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU) {
            float v_b = proc.params[item.param_id], v_e = proc.effective_params[item.param_id];
            if (item.param_id == PARAM_MAP_AMT) {
                float mv = proc.knob_map_amounts[proc.edit_param_target];
                snprintf(value_str, 16, "%d%%", (int)(mv * 100.f));
                n_base = n_eff = (mv + 1.f) * 0.5f;
            } else {
                switch(item.param_id) {
                    case PARAM_PRE_GAIN: case PARAM_POST_GAIN: snprintf(value_str, 16, "%d%%", (int)(v_b * 200.f)); break;
                    case PARAM_MIX: case PARAM_FEEDBACK: case PARAM_SEND: case PARAM_STEREO: snprintf(value_str, 16, "%d%%", (int)(v_b * 100.f)); break;
                    case PARAM_BPM: snprintf(value_str, 16, "%d", (int)v_b); break;
                    case PARAM_DIVISION: snprintf(value_str, 16, "1/%d", (int)v_b); break;
                    case PARAM_PITCH: { float st = 12.f * log2f(v_b); snprintf(value_str, 16, "%+d.%dst", (int)st, (int)(fabsf(st*10.f))%10); break; }
                    case PARAM_GRAIN_SIZE: snprintf(value_str, 16, "%dms", (int)(v_b * 1000.f)); break;
                    case PARAM_GRAIN_DENSITY: snprintf(value_str, 16, "%dHz", (int)v_b); break;
                }
                n_base = GetNormVal(item.param_id, v_b, proc.division_idx);
                n_eff  = GetNormVal(item.param_id, v_e, proc.division_idx);
            }
        }

        if (edit) {
            snprintf(label_str, 32, "%s", value_str);
            DrawHighlightBox(display, kTextColX, y - 1, kTextColWidth, 10, true);
        } else snprintf(label_str, 32, "%s", item.name);

        if (sel) DrawSelectionIndicator(y, edit);
        DrawStringRot180(display, kTextColX + 1, y, label_str, Font_7x10, !edit);
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU) DrawValueBar(y, n_base, n_eff);
    }
    display.Update();
}