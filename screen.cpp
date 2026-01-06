#include "screen.h"
#include <cstdio>
#include <cstring>

using namespace daisy;

static OledDisplay<SSD130xI2c128x64Driver> display;

// --- HELPERS ---

static void DrawRectRot180(OledDisplay<OledDriver> &disp, int x, int y, int w, int h, bool on)
{
    for(int i = 0; i < w; i++)
    {
        for(int j = 0; j < h; j++)
        {
            int rx = disp.Width() - 1 - (x + i);
            int ry = disp.Height() - 1 - (y + j);
            if(rx >= 0 && ry >= 0 && rx < (int)disp.Width() && ry < (int)disp.Height())
            {
                disp.DrawPixel(rx, ry, on);
            }
        }
    }
}

static void DrawStringRot180(OledDisplay<OledDriver> &disp, int x, int y, const char *str, const FontDef &font, bool on, bool invert_bg, bool full_width = false, int h_padding = 1, int v_padding = 0)
{
    int str_w = 0;
    const char* s = str;
    while(*s++) str_w += font.FontWidth;
    
    if (invert_bg)
    {
        int bg_x, bg_y, bg_w, bg_h;
        bg_y = y - v_padding;
        bg_h = font.FontHeight + (v_padding * 2);

        if (full_width)
        {
            bg_x = 0;
            bg_w = disp.Width();
        }
        else
        {
            bg_x = x - h_padding;
            bg_w = str_w + (h_padding * 2);
        }

        DrawRectRot180(disp, bg_x, bg_y, bg_w, bg_h, on);
        on = !on;
    }

    int cx = x;
    while(*str)
    {
        if(*str >= 32 && *str <= 126)
        {
            for(int i = 0; i < (int)font.FontHeight; i++)
            {
                uint32_t row = font.data[(*str - 32) * font.FontHeight + i];
                for(int j = 0; j < (int)font.FontWidth; j++)
                {
                    if((row << j) & 0x8000)
                    {
                        disp.DrawPixel(disp.Width() - 1 - (cx + j), disp.Height() - 1 - (y + i), on);
                    }
                }
            }
        }
        cx += font.FontWidth;
        ++str;
    }
}

static void DrawBarModRot180(OledDisplay<OledDriver> &disp, int x, int y, int w, int h, float base, float effective, bool on)
{
    int base_w = (int)(w * base);
    int eff_w  = (int)(w * effective);
    if(base_w < 0) base_w = 0; if(base_w > w) base_w = w;
    if(eff_w < 0) eff_w = 0; if(eff_w > w) eff_w = w;

    for(int i=0; i<w; i++)
    {
        int solid_limit = (base_w < eff_w) ? base_w : eff_w;
        bool is_solid   = (i < solid_limit);
        bool is_mod_pos = (i >= base_w && i < eff_w);
        bool is_mod_neg = (i >= eff_w && i < base_w);
        bool is_border  = (i == 0 || i == w - 1);

        for(int j=0; j<h; j++)
        {
            bool pixel_on = false;
            bool is_tb_border = (j == 0 || j == h - 1);
            
            if (is_border || is_tb_border) pixel_on = true;
            else if (is_solid) pixel_on = true;
            else if (is_mod_pos) {
                int p = i % 4; int mid = h / 2;
                if ((p==0 && (j==mid-2 || j==mid+2)) || (p==1 && (j==mid-1 || j==mid+1)) || (p==2 && j==mid)) pixel_on = true;
            }
            else if (is_mod_neg) {
                int p = i % 4; int mid = h / 2;
                if ((p==2 && (j==mid-2 || j==mid+2)) || (p==1 && (j==mid-1 || j==mid+1)) || (p==0 && j==mid)) pixel_on = true;
            }
            if(pixel_on) disp.DrawPixel(disp.Width() - 1 - (x + i), disp.Height() - 1 - (y + j), on);
        }
    }
}

// --- MAIN CLASS ---

void Screen::Init(DaisySeed &seed)
{
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.mode   = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.speed  = I2CHandle::Config::Speed::I2C_1MHZ;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed.GetPin(11);
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

void Screen::DrawStatus(Processing& proc)
{
    if(blink_active && (System::GetNow() - blink_start) < 50) {
        display.Fill(true); display.Update(); return;
    }
    blink_active = false;
    display.Fill(false);
    
    char line[32];

    // --- 1. ROOT LEVEL VIEW (Nodes) ---
    if (proc.current_node_idx == -1)
    {
        int item_h = 14; 
        int total_h = Processing::NODE_COUNT * item_h;
        int start_y = (64 - total_h) / 2;

        if(proc.node_settings)
        {
             DrawStringRot180(display, 0, 0, "[SETTINGS]", Font_7x10, true, false);
             DrawStringRot180(display, 0, 15, "Node Setup...", Font_6x8, true, false);
        }
        else
        {
            for(int i=0; i<Processing::NODE_COUNT; i++)
            {
                bool sel = (i == proc.root_cursor);
                int y = start_y + i * item_h;
                
                // USE POINTER SYNTAX: proc.nodes[i]->name
                snprintf(line, sizeof(line), " %s", proc.nodes[i]->name);
                DrawStringRot180(display, 0, y, line, Font_7x10, true, sel, true, 1, 2); 
            }
        }
    }
    // --- 2. NODE PARAMETER VIEW ---
    else
    {
        // USE POINTER SYNTAX
        NodeBase* node = proc.nodes[proc.current_node_idx];
        
        // Header
        if(proc.advanced_mode) {
             if(proc.adv_cursor == -1) DrawStringRot180(display, 0, 0, " < BACK", Font_7x10, true, true, true, 1, 2);
             else DrawStringRot180(display, 0, 0, " ADVANCED", Font_7x10, true, false);
        } else if(proc.param_cursor == -1) {
             DrawStringRot180(display, 0, 0, " < BACK", Font_7x10, true, true, true, 1, 2); 
        } else if(proc.edit_state) {
             DrawStringRot180(display, 0, 0, " EDITING", Font_7x10, true, false);
        } else {
             snprintf(line, sizeof(line), " %s", node->name);
             DrawStringRot180(display, 0, 0, line, Font_7x10, true, false);
        }

        int y_list_start = 14; 
        int row_h = 12;
        int bar_h = 10;
        int bar_y_off = 1;

        if (proc.advanced_mode)
        {
            auto parent = node->params[proc.viewing_param];
            
            // Map Row
            bool sel_map = (proc.adv_cursor == 0);
            int pct = (int)(node->map_amounts[proc.viewing_param] * 100.0f);
            
            int y_text_map = y_list_start + 2;

            snprintf(line, sizeof(line), " Map Amt");
            DrawStringRot180(display, 2, y_text_map, line, Font_6x8, true, sel_map, false, 2, 2);
            
            snprintf(line, sizeof(line), "%s%d%%", (pct > 0 ? "+" : ""), pct);
            DrawStringRot180(display, 80, y_text_map, line, Font_6x8, true, sel_map && proc.edit_state, false, 2, 2);

            // Child Row
            if (parent.has_child)
            {
                auto child = node->params[parent.child_id];
                bool sel_child = (proc.adv_cursor == 1);
                int y_child = y_list_start + row_h;
                int y_text_child = y_child + 2;

                snprintf(line, sizeof(line), " %s", child.name);
                DrawStringRot180(display, 2, y_text_child, line, Font_6x8, true, sel_child, false, 2, 2);
                
                DrawBarModRot180(display, 60, y_child + bar_y_off, 60, bar_h, child.base_value, child.effective_value, true);
                if(sel_child && proc.edit_state) DrawStringRot180(display, 122, y_text_child, "<", Font_6x8, true, false);
            }
        }
        else
        {
            // Normal List
            for(int i=0; i<node->param_count; i++)
            {
                int y = y_list_start + (i * row_h);
                bool is_sel = (i == proc.param_cursor);
                int y_text = y + 2;

                snprintf(line, sizeof(line), " %s", node->params[i].name);
                DrawStringRot180(display, 2, y_text, line, Font_6x8, true, is_sel, false, 2, 2);
                
                DrawBarModRot180(display, 50, y + bar_y_off, 70, bar_h, 
                                 node->params[i].base_value, 
                                 node->params[i].effective_value, true);
            }
        }
    }
    display.Update();
}