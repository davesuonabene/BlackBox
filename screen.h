#pragma once
#include "hid/disp/oled_display.h"
#include "dev/oled_ssd130x.h"
#include "util/oled_fonts.h"
#include "daisy_seed.h"
#include "processing.h" // Include this to know about Processing

using OledDriver = daisy::SSD130xI2c128x64Driver;

struct Screen
{
    bool     blink_active = false;
    uint32_t blink_start  = 0;

    void Init(daisy::DaisySeed &seed);

    void Blink(uint32_t now);

    // Updated signature
    void DrawStatus(Processing &proc); 
};