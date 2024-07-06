/*
 * tinwi.cpp
 *
 *  Created on: 13 июн. 2024 г.
 *      Author: layst
 */

#include "tinwi.h"
#include "BaseSequencer.h"
#include "ws2812b.h"
#include "MsgQ.h"

static const int32_t kLedCntTotal = FLAME_LEN * BAND_CNT;
static const int32_t kTinwiCntMax = kLedCntTotal;
static const int32_t kFramePeriod = 7;
static const int32_t kBrtMax = 255;
static const int32_t kOnOfSmooth = 450L;

extern Neopixels_t leds;
ColorHSV_t ClrBattery {0, 0, 0};

Params param_set[] = {
        {
                270, // smooth_min
                540, // smooth_max
                45,  // hsv_v_min
                100, // hsv_v_max
                117, // hsv_h_min
                122, // hsv_h_max
                18,  // tinwi_cnt
                4    // off_v
        },
};
//static const uint32_t kparam_set_cnt = countof(param_set);

class Tinwe : public BaseSequencer_t<LedHSVChunk_t> {
private:
    ColorHSV_t curr_hsv{120, 100, 4};
    LedHSVChunk_t lsq[3] = { {Chunk::Setup}, {Chunk::Setup}, {Chunk::End} };
public:
    int32_t led_indx = -1;
    void GenerateAndStart();
    // Base Sequencer's
    void SwitchOff() { leds.ClrBuf[led_indx] = clBlack; curr_hsv.DWord32 = 0; }
    bool AdjustAndCheckIfGotoNextChunk() {
        ColorHSV_t target_hsv = pcurrent_chunk->color; // To make things shorter
        if(curr_hsv == target_hsv) return true;
        // Not equal
        if(pcurrent_chunk->smooth_value == 0) { // If smooth is zero,
            curr_hsv = target_hsv;  // set color now and goto next chunk
            return true;
        }
        else { // smooth_value != 0, adjust it
            curr_hsv.Adjust(target_hsv);
            // Check if equal now
            if(curr_hsv == target_hsv) return true;
            // Not equal, calculate time to next adjustment
            int32_t delay = curr_hsv.DelayToNextAdj(target_hsv, pcurrent_chunk->smooth_value);
            SetupDelay(delay);
            return false;
        } // smooth_value != 0
    }
    void SetCurrRGB() { leds.ClrBuf[led_indx] = curr_hsv.ToRGB(); }
};

static Tinwe tinwi[kTinwiCntMax];

static inline bool TinweIndxIsOccupied(int32_t indx) {
    for(Tinwe &tinwe : tinwi)
        if(tinwe.led_indx == indx) return true;
    return false;
}

void Tinwe::GenerateAndStart() {
    // Get new led indx
    while(true) {
        int32_t new_indx = Random::Generate(0, (kLedCntTotal-1));
        if(!TinweIndxIsOccupied(new_indx)) {
            led_indx = new_indx;
            break;
        }
    }
//    led_indx = 36; // DEBUG
    // Generate new params
    lsq[0].smooth_value = Tinwi::curr_params->GetSmooth();
    lsq[0].color.H = Tinwi::curr_params->GetH();
    lsq[0].color.S = CLR_HSV_S_MAX;
    lsq[0].color.V = Tinwi::curr_params->GetV();
    lsq[1].smooth_value = Tinwi::curr_params->GetSmooth();
    lsq[1].color = lsq[0].color;
    lsq[1].color.V = Tinwi::curr_params->off_v;
    // Start it
    StartOrRestart(lsq);
}

#if 1 // ============================ On-Off Layer =============================
enum PhaseState_t {stIdle, stFadingOut, stFadingIn, stStopping} PhaseState;
int32_t OnOffBrt = 0;
virtual_timer_t IOnOffTmr;

void StartTimerI(uint32_t ms);

void OnOffTmrCallback(void *p) {
    chSysLockFromISR();
    switch(PhaseState) {
        case stFadingIn:
            if(OnOffBrt == kBrtMax) PhaseState = stIdle;
            else {
                OnOffBrt++;
                StartTimerI(ClrCalcDelay(OnOffBrt, kOnOfSmooth));
            }
            break;
        case stFadingOut:
            if(OnOffBrt == 0) {
                PhaseState = stIdle;
                EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdLedsDone));
            }
            else {
                OnOffBrt--;
                StartTimerI(ClrCalcDelay(OnOffBrt, kOnOfSmooth));
            }
            break;
        default: break;
    }
    chSysUnlockFromISR();
}

void StartTimerI(uint32_t ms) { chVTSetI(&IOnOffTmr, TIME_MS2I(ms), OnOffTmrCallback, nullptr); }
void StopTimer() { chVTReset(&IOnOffTmr); }
#endif

static THD_WORKING_AREA(waEffThread, 512);
static void EffThread(void *arg) {
    chRegSetThreadName("Tinwi");
//    uint32_t time_passed = 0, params_indx = 0;;
    while(true) {
        chThdSleepMilliseconds(kFramePeriod);
//        leds.SetAll((Color_t){0,0,0,0}); // Clear buffer before proceeding

        // Show charge if needed
        if(ClrBattery.V != 0) {
            leds.SetAll(ClrBattery.ToRGB());
            leds.SetCurrentColors();
            chThdSleepMilliseconds(1530);
            leds.SetAll(clBlack);
            ClrBattery.V = 0; // Do not show next time
        }

        // ==== Tinwi ====
        for(int32_t i=0; i<Tinwi::curr_params->tinwi_cnt; i++) {
            if(tinwi[i].IsIdle()) tinwi[i].GenerateAndStart(); // Check if new generation required
            tinwi[i].SetCurrRGB(); // Draw them
        }

        // ==== On-Off Layer ====
        for(Color_t &Clr : leds.ClrBuf) Clr.SetRGBBrightness(OnOffBrt, kBrtMax);

        // ==== Draw it ====
        leds.SetCurrentColors();

        // Check if change params
//        time_passed += kFramePeriod;
//        if(time_passed > 9000) {
//            time_passed = 0;
//            params_indx++;
//            if(params_indx >= kparam_set_cnt) params_indx = 0;
//            curr_params = &param_set[params_indx];
//        }
    } // while true
}

namespace Tinwi {

void Init() {
    chThdCreateStatic(waEffThread, sizeof(waEffThread), NORMALPRIO, (tfunc_t)EffThread, nullptr);
}

void FadeIn() {
    PhaseState = stFadingIn;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, kOnOfSmooth));
    chSysUnlock();
}

void FadeOut() {
    PhaseState = stFadingOut;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, kOnOfSmooth));
    chSysUnlock();
}

void StopNow() {
    StopTimer();
    PhaseState = stStopping;
    OnOffBrt = 0;
//    MustStop = true;
}

void ShowCharge(ColorHSV_t hsv) { ClrBattery = hsv; }

Params *curr_params = &param_set[0];
} // namespace

