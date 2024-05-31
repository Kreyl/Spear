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
#define SMOOTH_VAR  180
#define START_CLR   clYellow
// Do not touch
#define BRT_MAX     255L

#if 1 // ======= OnOff Layer =======
void OnOffTmrCallback(void *p);

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
        // Reset colors
        Leds.SetAll(Color_t{0,0,0,0});
        // Process OnOff
        BackLayer.Apply();
        // Show it
        Leds.SetCurrentColors();
    }
}

namespace Eff {
void Init() {
    chThdCreateStatic(waNpxThread, sizeof(waNpxThread), NORMALPRIO, (tfunc_t)NpxThread, nullptr);
}

void SetBackColor(Color_t AClr) { BackLayer.SetColor(AClr); }

void FadeIn()  { BackLayer.FadeIn();  }
void FadeOut() { BackLayer.FadeOut(); }

} // namespace
