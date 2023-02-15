/*
 * Effects.cpp
 *
 *  Created on: 5 dec 2019
 *      Author: Kreyl
 */

#include "Effects.h"
#include "ch.h"
#include "kl_lib.h"
#include "color.h"
#include "ws2812b.h"
#include "MsgQ.h"
#include "board.h"

extern Neopixels_t Leds;

// On-off layer
#define SMOOTH_VAR      180

// Do not touch
#define BRT_MAX     255L

// Indx 0 is top of the head
//static void SetSnakeColor(int32_t Indx, Color_t Clr) {
//    if(Indx < 0 or Indx >= SNAKE_LED_CNT) return;
//    Indx += SNAKE_START_INDX;
//    Leds.ClrBuf[Indx] = Clr;
//}
//
//static void SetMoonColor(int32_t Indx, Color_t Clr) {
//    if(Indx < 0 or Indx >= MOON_LED_CNT) return;
//    Indx += MOON_START_INDX;
//    Leds.ClrBuf[Indx] = Clr;
//}

//void MixSnakeToBuf(Color_t Clr, int32_t Brt, int32_t Indx) {
//    SetSnakeColor(Indx, Color_t(Clr, BackClr, Brt));
//}
//void MixMoonToBuf(Color_t Clr, int32_t Brt, int32_t Indx) {
//    SetMoonColor(Indx, Color_t(Clr, BackClr, Brt));
//}

#if 1 // ======= Flash =======
#define FLASH_CLR       (Color_t(0, 255, 9))
#define FLASH_CNT       3
#define FLASH_END_INDX  11

void FlashTmrCallback(void *p);

class Flash_t {
private:
    int32_t IndxStart, Len;
    uint32_t DelayUpd_ms;  // Delay between updates
    virtual_timer_t ITmr;
    void StartTimerI(uint32_t ms) {
        chVTSetI(&ITmr, TIME_MS2I(ms), FlashTmrCallback, this);
    }
    systime_t strt;
public:
    int32_t EndIndx = FLASH_END_INDX; // which Indx to touch to consider flash ends
    Color_t Clr = FLASH_CLR;
    void Apply() {
        for(int32_t i=0; i<Len; i++) {
//            MixToBuf(Clr, ((BRT_MAX * (Len - i)) / Len), IndxStart - i);
        }
    }

    void GenerateI(uint32_t DelayBefore_ms) {
        PrintfI("%u Dur: %u\r", this, TIME_I2MS(chVTTimeElapsedSinceX(strt)));
        strt = chVTGetSystemTimeX();
        DelayUpd_ms = 90; // Random::Generate(36, 63);
        IndxStart = -1;
//        Len = 11;
        Len = Random::Generate(9, 12);
//        Len = Random::Generate(14, 18);
        // Start delay before
        StartTimerI(DelayBefore_ms);
    }

    void OnTmrI() {
        IndxStart++; // Time to move
        // Check if path completed
        if((IndxStart - Len) > EndIndx) GenerateI(18);
        else StartTimerI(DelayUpd_ms);
    }
};

void FlashTmrCallback(void *p) {
    chSysLockFromISR();
    ((Flash_t*)p)->OnTmrI();
    chSysUnlockFromISR();
}

Flash_t FlashBuf[FLASH_CNT];
#endif

#if 1 // ======= OnOff Layer =======
void OnOffTmrCallback(void *p);

//static uint32_t CalcDelayFromClrs(Color_t Clr1, Color_t Clr2) {
//    uint8_t v = Clr1.R < Clr2.R? Clr1.R : Clr2.R;
//    uint32_t dmax = ClrCalcDelay(v, SMOOTH_VAR);
//    v = Clr1.G < Clr2.G? Clr1.G : Clr2.G;
//    uint32_t d2 = ClrCalcDelay(v, SMOOTH_VAR);
//    if(d2 > dmax) dmax = d2;
//    v = Clr1.B < Clr2.B? Clr1.B : Clr2.B;
//    d2 = ClrCalcDelay(v, SMOOTH_VAR);
//    if(d2 > dmax) dmax = d2;
//    return dmax;
//}

class BackLayer_t {
private:
    bool IsIdle;
    virtual_timer_t ITmr;
    void StartTimerI(uint32_t ms) {
        chVTSetI(&ITmr, TIME_MS2I(ms), OnOffTmrCallback, nullptr);
    }
    void StartTimerOrSendEvtI() {
        if(ICurrClr == ITargetClr) EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdLedsDone));
        else StartTimerI(ICurrClr.DelayToNextAdj(ITargetClr, SMOOTH_VAR));
    }
    Color_t ITargetClr = clBlack, ICurrClr = clBlack, ISavedColor = START_CLR;
public:
    void Apply() { Leds.SetAll(ICurrClr); }

    void SetColor(Color_t AClr) {
        chSysLock();
        ITargetClr = AClr;
        ISavedColor = AClr;
        StartTimerOrSendEvtI();
        chSysUnlock();
    }

    void FadeIn() {
        chSysLock();
        ITargetClr = ISavedColor;
        StartTimerOrSendEvtI();
        chSysUnlock();
    }

    void FadeOut() {
        chSysLock();
        ITargetClr = clBlack;
        StartTimerOrSendEvtI();
        chSysUnlock();
    }

    void OnTmrI() {
        ICurrClr.Adjust(ITargetClr);
        StartTimerOrSendEvtI();
    }
} BackLayer;

void OnOffTmrCallback(void *p) {
    chSysLockFromISR();
    BackLayer.OnTmrI();
    chSysUnlockFromISR();
}
#endif

// Thread
static THD_WORKING_AREA(waNpxThread, 512);
__noreturn
static void NpxThread(void *arg) {
    chRegSetThreadName("Npx");
    while(true) {
        chThdSleepMilliseconds(7);
        Leds.SetAll(Color_t{0,0,0,0});// Clear buffer before proceeding
        // Process OnOff
        BackLayer.Apply();
        // Show it
        Leds.SetCurrentColors();
    }
}

namespace Eff {
void Init() {
//    for(uint32_t i=0; i<FLASH_CNT; i++) {
//        chSysLock();
//        FlashBuf[i].GenerateI(i * 1107 + 9);
//        chSysUnlock();
//    }
    chThdCreateStatic(waNpxThread, sizeof(waNpxThread), NORMALPRIO, (tfunc_t)NpxThread, nullptr);

//    int indx = 0;
//    for(int i=0; i<28; i++) Leds.ClrBuf[indx++] = clGreen;
//    for(int i=0; i<20; i++) Leds.ClrBuf[indx++] = clRed;
//    for(int i=0; i<55; i++) Leds.ClrBuf[indx++] = clBlue;

//    Leds.SetCurrentColors();
}

//void SetColor(Color_t AClr) {
////    for(uint32_t i=0; i<FLASH_CNT; i++) FlashBuf[i].Clr = AClr;
//}

void SetBackColor(Color_t AClr) { BackLayer.SetColor(AClr); }

void FadeIn()  { BackLayer.FadeIn();  }
void FadeOut() { BackLayer.FadeOut(); }

} // namespace
