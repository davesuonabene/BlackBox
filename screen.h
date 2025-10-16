#pragma once
#include "hid/disp/oled_display.h"
#include "dev/oled_ssd130x.h"
#include "util/oled_fonts.h"
#include "daisy_seed.h"

using OledDriver = daisy::SSD130xI2c128x64Driver;

struct Screen
{
    bool     blink_active = false;
    uint32_t blink_start  = 0;

    void Init(daisy::DaisySeed &seed);

    void Blink(uint32_t now);

    void DrawStatus(int      mix_pct,
                    int      fbk_pct,
                    uint32_t delay_ms,
                    int      bpm,
                    int      division,
                    int      selected_param,
                    int      master_mix,
                    int      master_fbk,
                    int      master_bpm,
                    int      master_division,
                    bool     rotated180);
};


