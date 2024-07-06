/*
 * Settings.h
 *
 *  Created on: 20 нояб. 2022 г.
 *      Author: layst
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <inttypes.h>
#include "color.h"
#include <vector>

//#define DEBUG   TRUE

#define SETTINGS_MAX_CNT        4

#define CORE_MAX_SZ             FLAME_LEN
#define SPARKS_MODE_RANDOM      0
#define SPARKS_MODE_GRADIENT    1
#define SPARK_CENTER_SZ         1
#define SPARK_TAIL_LEN_MAX      27
#define SPARK_LEN_MAX           (SPARK_CENTER_SZ + SPARK_TAIL_LEN_MAX * 2)
#define SPARKS_CNT_MAX          99

struct FlameSettings {
    struct {
        uint8_t Sz = 10;
        uint16_t ClrHMin = 120, ClrHMax = 120;
        uint8_t ClrV = 18;
        bool IsOk() { return Sz <= CORE_MAX_SZ and ClrHMax <= 360 and ClrHMin <= 360 and ClrV <= 100; }
    } Core;
    struct {
        uint8_t Cnt = 3;
        uint8_t TailLen = 18;
        uint16_t ClrHMin = 120, ClrHMax = 140;
        uint8_t ClrV = 100;
        uint16_t DelayBeforeRestart = 630;
        int16_t AccMin = 4, AccMax = 9;
        int16_t StartDelayMin = 99, StartDelayMax = 153;
        uint8_t Mode = 0;
        bool IsOk() { return Cnt <= SPARKS_CNT_MAX and TailLen < SPARK_TAIL_LEN_MAX and ClrHMax <= 360 and ClrHMin <= 360 and ClrV <= 100
                and AccMin < 99 and AccMax < 99; }
        uint16_t GetRandomHue() {
            if(ClrHMin < ClrHMax) return Random::Generate(ClrHMin, ClrHMax);
            else return Random::Generate(ClrHMax, ClrHMin);
        }
    } Sparks;

    bool IsOk() { return Core.IsOk() and Sparks.IsOk(); }
} __attribute__ ((aligned));

#endif // SETTINGS_H_
