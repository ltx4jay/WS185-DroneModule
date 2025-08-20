#pragma once

#include <stdint.h>

extern struct Options
{
    bool    gimbalOn   = false;
    uint8_t brightness = 40;
} gOpts;

extern void (*screenLoop)(void);
void refreshScreen();
void setBrightness(uint8_t percent);

void NVS_Store();