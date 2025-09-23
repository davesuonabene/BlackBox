#include "screen.h"
#include "hid/disp/oled_display.h"
#include <cstdio>

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

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

void Screen::DrawStatus(int      mix_pct,
                        int      fbk_pct,
                        uint32_t delay_ms,
                        int      selected_param,
                        int      master_mix,
                        int      master_fbk,
                        int      master_delay,
                        bool     rotated180)
{
    if(blink_active && (System::GetNow() - blink_start) < 120)
    {
        display.Fill(true);
        display.Update();
        return;
    }
    if(blink_active)
        blink_active = false;

    display.Fill(false);
    char line[48];
    auto MasterStr = [](int m) -> const char*
    {
        switch(m)
        {
            case 1: return " (P1)";
            case 2: return " (P2)";
            default: return "";
        }
    };
    if(rotated180)
    {
        DrawStringRot180(display, 0, 0, "BlackBox", Font_7x10, true);
        snprintf(line, sizeof(line), "%sMix: %3d%%%s",
                 selected_param == 0 ? "> " : "  ", mix_pct, MasterStr(master_mix));
        DrawStringRot180(display, 0, 14, line, Font_6x8, true);
        snprintf(line, sizeof(line), "%sFbk: %3d%% %s",
                 selected_param == 1 ? "> " : "  ", fbk_pct, MasterStr(master_fbk));
        DrawStringRot180(display, 0, 24, line, Font_6x8, true);
        snprintf(line, sizeof(line), "%sDelay: %lu ms%s",
                 selected_param == 2 ? "> " : "  ", (unsigned long)delay_ms, MasterStr(master_delay));
        DrawStringRot180(display, 0, 34, line, Font_6x8, true);
        DrawStringRot180(display, 0, 44, "Hold Enc1/Enc2: assign Pot1/Pot2", Font_6x8, true);
    }
    else
    {
        display.SetCursor(0, 0);
        display.WriteString("BlackBox", Font_7x10, true);
        display.SetCursor(0, 14);
        snprintf(line, sizeof(line), "%sMix: %3d%%%s",
                 selected_param == 0 ? "> " : "  ", mix_pct, MasterStr(master_mix));
        display.WriteString(line, Font_6x8, true);
        display.SetCursor(0, 24);
        snprintf(line, sizeof(line), "%sFbk: %3d%% %s",
                 selected_param == 1 ? "> " : "  ", fbk_pct, MasterStr(master_fbk));
        display.WriteString(line, Font_6x8, true);
        display.SetCursor(0, 34);
        snprintf(line, sizeof(line), "%sDelay: %lu ms%s",
                 selected_param == 2 ? "> " : "  ", (unsigned long)delay_ms, MasterStr(master_delay));
        display.WriteString(line, Font_6x8, true);
        display.SetCursor(0, 44);
        display.WriteString("Hold Enc1/Enc2: assign Pot1/Pot2", Font_6x8, true);
    }
    display.Update();
}


