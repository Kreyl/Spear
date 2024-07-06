/*
 * Flame.cpp
 *
 *  Created on: 12 нояб. 2022 г.
 *      Author: layst
 */

#include "Flame.h"
#include "ch.h"
#include "MsgQ.h"
#include "ws2812b.h"
#include <vector>

Flames flames;
extern Neopixels_t leds;
static FlameSettings ISettings;

// Set pix, mixing it with current color
inline void SetPixRing(int32_t x, Color_t Clr) {
//    Printf("%d\r", x);
    if (x >= FLAME_LEN or x < 0) return; // too far
    leds.ClrBuf[x].MixAddingRGB(Clr); // First band
    leds.ClrBuf[(FLAME_LEN * 2 - 1) - x] = leds.ClrBuf[x]; // Second band
    leds.ClrBuf[(FLAME_LEN * 2) + x] = leds.ClrBuf[x]; // Third band
}

#if 1 // ============================ Spark ====================================
void SparkTmrCallback(void *p);

// Proportion for gradients calculation
uint16_t x2Value(uint16_t x, uint16_t fMin, uint16_t fMax) { return Proportion<uint16_t>(0, (FLAME_LEN-1), fMin, fMax, x); }

// Tail Brightness for short tails
#define BRT_TAIL1_1     128
#define BRT_TAIL2_1     85
#define BRT_TAIL2_2     170
#define BRT_TAIL_END    2L

class Spark_t {
private:
    int16_t MoveDelay_ms = 81;
    int16_t Acceleration = 27;
    int32_t x = -1, xMax = 0;
    virtual_timer_t itmr_move;
    systime_t time_to_start_st = 540;
    std::vector<Color_t> IBuf;
public:
    void Init() { // Recreate IBuf making it's colors black
        IBuf.clear();
        if(ISettings.Sparks.Mode == SPARKS_MODE_RANDOM)
            IBuf.resize(ISettings.Sparks.TailLen + SPARK_CENTER_SZ + ISettings.Sparks.TailLen);
    }

    void Sleep() {
        x = -1; // Sleeping
        time_to_start_st = chVTGetSystemTimeX() + TIME_MS2I(Random::Generate(0, ISettings.Sparks.DelayBeforeRestart));
    }

    void Generate() {
        if(ISettings.Sparks.Mode == SPARKS_MODE_RANDOM) { // Prepare buf which is spark's image
            xMax = (FLAME_LEN - 1) + (int32_t)IBuf.size();
            int32_t N = 0, TailLen = ISettings.Sparks.TailLen, MaxV = ISettings.Sparks.ClrV, BrtRGB;
            ColorHSV_t hsv{ISettings.Sparks.GetRandomHue(), 100, (uint8_t)MaxV};
            Color_t rgb = hsv.ToRGB();
            MaxV = (MaxV * 255L) / 100L; // [0;100] -> [0;255]

            // Head
            for(int32_t i=0; i<TailLen; i++) {
                if(TailLen == 1) BrtRGB = BRT_TAIL1_1;
                else if(TailLen == 2) BrtRGB = (i == 0)? BRT_TAIL2_1 : BRT_TAIL2_2;
                else BrtRGB = Proportion<int32_t>(0, TailLen, BRT_TAIL_END, MaxV, i); // [0;TailLen) -> [1; MaxV)
                IBuf[N] = rgb;
                IBuf[N].SetRGBBrightness(BrtRGB, BRT_MAX);
                IBuf[N].ApplyGammaCorrectionRGB();
                N++;
            }

            // Center
            for(int32_t i=0; i<SPARK_CENTER_SZ; i++) {
                IBuf[N] = rgb;
                IBuf[N].ApplyGammaCorrectionRGB();
                N++;
            }

            // Tail
            for(int32_t i=1; i<=TailLen; i++) {
                if(TailLen == 1) BrtRGB = BRT_TAIL1_1;
                else if(TailLen == 2) BrtRGB = (i == 1)? BRT_TAIL2_2 : BRT_TAIL2_1;
                else BrtRGB = Proportion<int32_t>(0, TailLen, MaxV, BRT_TAIL_END, i); // [0;TailLen) -> [MaxV; 1)
                IBuf[N] = rgb;
                IBuf[N].SetRGBBrightness(BrtRGB, BRT_MAX);
                IBuf[N].ApplyGammaCorrectionRGB();
                N++;
            }
        }
        else { // Gradient
            xMax = FLAME_LEN + ISettings.Sparks.TailLen;
        }
        // === Init coord, speed, acceleration ===
        x = 0;
        MoveDelay_ms = Random::Generate(ISettings.Sparks.StartDelayMin, ISettings.Sparks.StartDelayMax);
        if(MoveDelay_ms < SPARK_DELAY_MIN) MoveDelay_ms = SPARK_DELAY_MIN;
        Acceleration = Random::Generate(ISettings.Sparks.AccMin, ISettings.Sparks.AccMax);
        chVTSet(&itmr_move, TIME_MS2I(MoveDelay_ms), SparkTmrCallback, this);
    }

    void Process() {
        if(x == -1) {
            if(chVTGetSystemTimeX() >= time_to_start_st) Generate();
            else return;
        }
        // Draw it
        if(ISettings.Sparks.Mode == SPARKS_MODE_RANDOM) {
            int32_t Len = IBuf.size();
            int32_t Start = (x < FLAME_LEN)? 0 : x - (FLAME_LEN -1);
            int32_t Stop  = (x < Len)? x : Len - 1;
            int32_t xCurr = (x < FLAME_LEN)? x : (FLAME_LEN - 1);
            for(int32_t i=Start; i<=Stop; i++) SetPixRing(xCurr--, IBuf[i]);
        }
        else { // Gradient
            int32_t SparkLen = ISettings.Sparks.TailLen + 1;
            for(int32_t i=0; i<SparkLen; i++) {
                int32_t fx = x - i;
                if(fx < 0) break;
                if(fx >= FLAME_LEN) continue;
                ColorHSV_t hsv{x2Value(fx, ISettings.Sparks.ClrHMin, ISettings.Sparks.ClrHMax), 100, ISettings.Sparks.ClrV};
                Color_t rgb = hsv.ToRGB();
                SetPixRing(fx, rgb);
            }
        }
    }

    void StopI() {
        chVTResetI(&itmr_move); // IsArmed checked inside
    }

    void OnTmrI() {
        x++;
        if(x < xMax) {
            MoveDelay_ms -= Acceleration;
            if(MoveDelay_ms <= SPARK_DELAY_MIN) MoveDelay_ms = SPARK_DELAY_MIN;
            chVTSetI(&itmr_move, TIME_MS2I(MoveDelay_ms), SparkTmrCallback, this);
        }
        else Sleep();
    }
};

void SparkTmrCallback(void *p) {
    chSysLockFromISR();
    ((Spark_t*)p)->OnTmrI();
    chSysUnlockFromISR();
}

static std::vector<Spark_t> Sparks;
#endif

// ==== Thread ====
static THD_WORKING_AREA(waFlameThread, 512);
static void FlameThread(void *arg) {
    chRegSetThreadName("FlameThread");
    flames.IDraw();
}

void Flames::Init() {
    Sparks.clear();
    // Create and start thread
    chThdCreateStatic(waFlameThread, sizeof(waFlameThread), NORMALPRIO, (tfunc_t)FlameThread, nullptr);
}

void Flames::IDraw() {
    while(true) {
        chThdSleepMilliseconds(FRAME_PERIOD_ms);
        leds.SetAll((Color_t){0,0,0}); // Clear buffer before proceeding

        // Disable everything
        if(MustStop) {
            if(PhaseState == stStopping) {
                leds.SetCurrentColors();
                chSysLock();
                for(Spark_t &Spark : Sparks) Spark.StopI();
                chSysUnlock();
                PhaseState = stIdle;
            }
            else continue; // Just sleep forever
        }

        // Show charge if needed
        if(ClrBattery.V != 0) {
            for(uint32_t i=0; i<7; i++) leds.ClrBuf[i] = ClrBattery.ToRGB();
            leds.SetCurrentColors();
            chThdSleepMilliseconds(1530);
            for(uint32_t i=0; i<9; i++) leds.ClrBuf[i] = (Color_t){0,0,0,0};
            ClrBattery.V = 0; // Do not show next time
        }

        if(INewSettingsAppeared) ApplyNewSettings();

        // ==== Draw core ====
        for(uint32_t x=0; x<ISettings.Core.Sz; x++) {
            Color_t Clr;
            Clr.FromHSV(x2Value(x, ISettings.Core.ClrHMin, ISettings.Core.ClrHMax), 100, ISettings.Core.ClrV);
            SetPixRing(x, Clr);
        }
        // ==== Sparks Layer ====
        for(Spark_t &Spark : Sparks) Spark.Process();
        // ==== On-Off Layer ====
        for(Color_t &Clr : leds.ClrBuf) Clr.SetRGBWBrightness(OnOffBrt, BRT_MAX);

        // ==== Draw it ====
        leds.SetCurrentColors();
    } // while true
}

#if 1 // ============================ On-Off Layer =============================
void OnOffTmrCallback(void *p) {
    chSysLockFromISR();
    flames.OnOnOffTmrTickI();
    chSysUnlockFromISR();
}

void Flames::FadeIn() {
    PhaseState = stFadingIn;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
    chSysUnlock();
}

void Flames::FadeOut() {
    PhaseState = stFadingOut;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
    chSysUnlock();
}

void Flames::StopNow() {
    StopTimer();
    PhaseState = stStopping;
    OnOffBrt = 0;
    MustStop = true;
}

void Flames::OnOnOffTmrTickI() {
    switch(PhaseState) {
        case stFadingIn:
            if(OnOffBrt == BRT_MAX) PhaseState = stIdle;
            else {
                OnOffBrt++;
                StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
            }
            break;

        case stFadingOut:
            if(OnOffBrt == 0) {
                PhaseState = stIdle;
                EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdLedsDone));
            }
            else {
                OnOffBrt--;
                StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
            }
            break;

        default: break;
    }
}
#endif

void Flames::SetNewSettings(FlameSettings &ASettings) {
    chSysLock();
    INewSettings = ASettings;
    INewSettingsAppeared = true;
    chSysUnlock();
}

void Flames::ApplyNewSettings() {
    // Stop sparks
    chSysLock();
    INewSettingsAppeared = false;
    for(Spark_t &Spark : Sparks) Spark.StopI();
    ISettings = INewSettings;
    chSysUnlock();
    // Prepare sparks buf
    Sparks.resize(ISettings.Sparks.Cnt);
    // Prepare Sparks
    for(uint8_t j=0; j<ISettings.Sparks.Cnt; j++) {
        Sparks[j].Init();
        Sparks[j].Generate();
    }
}
