#include "screen.h"
#include <cstdio>
// ... [Headers/Constants/Helpers Unchanged] ...
// (Assume DrawCharRot180, DrawStringRot180, DrawHighlightBox, GetNormVal, DrawValueBar are here)

// [Paste the DrawStatus implementation below]

void Screen::DrawStatus(Processing &proc, Hardware &hw)
{
    if (proc.trigger_blink) { Blink(System::GetNow()); proc.trigger_blink = false; }
    display.Fill(false);
    if (blink_active && (System::GetNow() - blink_start < 100)) { display.Fill(true); display.Update(); return; }

    // [Draw Menu Items - Unchanged from previous version] ... 
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
        char value_str[16] = ""; char label_str[32] = ""; float n_base = 0.0f, n_eff = 0.0f;
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU) {
            float v_b = proc.params[item.param_id], v_e = proc.effective_params[item.param_id];
            if (item.param_id == PARAM_MAP_AMT) { float mv = proc.knob_map_amounts[proc.edit_param_target]; snprintf(value_str, 16, "%d%%", (int)(mv * 100.f)); n_base = n_eff = (mv + 1.f) * 0.5f; }
            else {
                switch(item.param_id) {
                    case PARAM_PRE_GAIN: case PARAM_POST_GAIN: snprintf(value_str, 16, "%d%%", (int)(v_b * 200.f)); break;
                    case PARAM_MIX: case PARAM_FEEDBACK: case PARAM_STEREO: case PARAM_SPRAY: snprintf(value_str, 16, "%d%%", (int)(v_b * 100.f)); break;
                    case PARAM_BPM: snprintf(value_str, 16, "%d", (int)v_b); break;
                    case PARAM_DIVISION: snprintf(value_str, 16, "1/%d", (int)v_b); break;
                    case PARAM_PITCH: { float st = 12.f * log2f(v_b); snprintf(value_str, 16, "%+d.%dst", (int)st, (int)(fabsf(st*10.f))%10); break; }
                    case PARAM_GRAIN_SIZE: snprintf(value_str, 16, "%dms", (int)(v_b * 1000.f)); break;
                    case PARAM_GRAINS: snprintf(value_str, 16, "%dHz", (int)v_b); break;
                }
                n_base = GetNormVal(item.param_id, v_b, proc.division_idx); n_eff  = GetNormVal(item.param_id, v_e, proc.division_idx);
            }
        }
        if (edit) { snprintf(label_str, 32, "%s", value_str); DrawHighlightBox(display, kTextColX, y - 1, kTextColWidth, 10, true); } else snprintf(label_str, 32, "%s", item.name);
        if (sel) DrawSelectionIndicator(y, edit);
        DrawStringRot180(display, kTextColX + 1, y, label_str, Font_7x10, !edit);
        if (item.type == TYPE_PARAM || item.type == TYPE_PARAM_SUBMENU) DrawValueBar(y, n_base, n_eff);
    }

    // --- Looper Status Bar ---
    int y_status = 54;
    const char* mode_str = "---";
    if (hw.looper_mode == Hardware::LP_RECORDING) mode_str = "REC";
    else if (hw.looper_mode == Hardware::LP_PLAYING) mode_str = "PLY";
    else if (hw.looper_mode == Hardware::LP_STOPPED) mode_str = "STP";

    DrawStringRot180(display, 0, y_status, mode_str, Font_7x10, true);

    if (hw.looper_mode != Hardware::LP_EMPTY)
    {
        float progress = 0.0f;
        // If Recording, visualize Recording Position vs Max (or vs Loop Length if known)
        if (hw.looper_mode == Hardware::LP_RECORDING) {
             progress = (float)hw.rec_pos / (float)LOOPER_MAX_SAMPLES;
        } 
        // If Playing, visualize Play Position vs Loop Length
        else if (hw.loop_length > 0) {
             progress = (float)hw.play_pos / (float)hw.loop_length;
        }

        int bar_x = 30; int bar_w = 98; int bar_h = 8;
        int rx_start = display.Width() - 1 - (bar_x + bar_w - 1);
        int rx_end   = display.Width() - 1 - bar_x;
        int ry_start = display.Height() - 1 - (y_status + bar_h - 1);
        int ry_end   = display.Height() - 1 - y_status;

        display.DrawRect(rx_start, ry_start, rx_end, ry_end, true, false);

        int fill_w = (int)(progress * (float)bar_w);
        if (fill_w > bar_w) fill_w = bar_w;
        if (fill_w > 0)
        {
             int rx_fill_start = display.Width() - 1 - (bar_x + fill_w - 1); 
             display.DrawRect(rx_fill_start, ry_start, rx_end, ry_end, true, true);
        }
    }

    display.Update();
}