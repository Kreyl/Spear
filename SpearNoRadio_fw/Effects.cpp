/*
 * Effects.cpp
 *
 *  Created on: 5 ???. 2019 ?.
 *      Author: Kreyl
 */

#include "Effects.h"
#include "ch.h"
#include "kl_lib.h"
#include "color.h"
#include "ws2812b.h"

extern Neopixels_t Leds;

#define PIX_PER_BAND    18
#define BAND_NUMBER     2

#define BRT_MAX     255L
#define BACK_CLR    (Color_t(255, 207, 0))
#define FLASH_CLR   (Color_t(255, 255, 255))
#define FLASH_CNT   4

static void SetColorRing(int32_t Indx, Color_t Clr) {
    if(Indx >= PIX_PER_BAND or Indx < 0) return;
    Leds.ClrBuf[Indx] = Clr;   // Always for first chunk
    // Iterate bands
    for(int32_t n=2; n <= BAND_NUMBER; n++) {
        int32_t i = (n & 1)? (PIX_PER_BAND * (n-1) + Indx) : (PIX_PER_BAND * n - 1 - Indx);
        Leds.ClrBuf[i] = Clr;
    }
}

void MixToBuf(Color_t Clr, int32_t Brt, int32_t Indx) {
    Printf("%u\r", Brt);
    SetColorRing(Indx, Color_t(FLASH_CLR, BACK_CLR, Brt));
}

#if 1 // ======= Flash =======
//#define

class Flash_t {
private:
    systime_t IStart = 0;
    Color_t Clr = FLASH_CLR;
    int32_t IndxStart, Len;
    uint32_t Delay_ms = 63;  // Delay between updates
public:
    void Generate() {
        Delay_ms = 180; // XXX Randomize
        IndxStart = -1; // XXX Randomize
        Len = 7; // XXX Randomize
    }
    void Update() {
        // Check if time to update
        if(chVTTimeElapsedSinceX(IStart) < Delay_ms) return;
        IStart = chVTGetSystemTimeX();
        // Check if path completed
        if((IndxStart - Len) > (PIX_PER_BAND + 7)) Generate();
        // Move it
        IndxStart++;
        // Draw it
        for(int32_t i=0; i<Len; i++) {
            MixToBuf(Clr, ((BRT_MAX * (Len - i)) / Len), IndxStart - i);
        }
    }
};

Flash_t FlashBuf[FLASH_CNT];
#endif

// Threads
static THD_WORKING_AREA(waNpxThread, 512);
__noreturn
static void NpxThread(void *arg) {
    chRegSetThreadName("Npx");
    while(true) {
        chThdSleepMilliseconds(45);
        // Reset colors
//        Leds.SetAll(BACK_CLR);
        // Iterate flashes
        for(Flash_t &IFlash : FlashBuf) IFlash.Update();
        // Show it
        Leds.SetCurrentColors();
    }
}


void EffInit() {
    for(Flash_t &IFlash : FlashBuf) IFlash.Generate();
    chThdCreateStatic(waNpxThread, sizeof(waNpxThread), NORMALPRIO, (tfunc_t)NpxThread, nullptr);
}

void EffFlashStart() {

}


