/*
 * Flame.h
 *
 *  Created on: 12 нояб. 2022 г.
 *      Author: layst
 */

#pragma once

#include <inttypes.h>
#include "ch.h"
#include "color.h"
#include "Settings.h"

#define SMOOTH_VAR              450L
#define BRT_MAX                 255L // Do not touch
#define FRAME_PERIOD_ms         7
#define SPARK_DELAY_MIN         18

void OnOffTmrCallback(void *p);

class Flames {
private:
    enum PhaseState_t {stIdle, stFadingOut, stFadingIn, stStopping} PhaseState;
    int32_t OnOffBrt = 0;
    virtual_timer_t IOnOffTmr;
    void StartTimerI(uint32_t ms) { chVTSetI(&IOnOffTmr, TIME_MS2I(ms), OnOffTmrCallback, nullptr); }
    void StopTimer() { chVTReset(&IOnOffTmr); }
    FlameSettings INewSettings;
    volatile bool INewSettingsAppeared = true, MustStop = false;
    ColorHSV_t ClrBattery {0, 0, 0};
    void ApplyNewSettings();
public:
    void Init();
    void FadeIn();
    void FadeOut();
    void StopNow();
    void SetNewSettings(FlameSettings &ASettings);
    void ShowCharge(ColorHSV_t hsv) { ClrBattery = hsv; }
    // Inner use
    void IDraw();
    void OnOnOffTmrTickI();
    friend class Spark_t;
};

extern Flames flames;
