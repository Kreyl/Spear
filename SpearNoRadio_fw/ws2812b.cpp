#include "ws2812b.h"

void NpxDmaDone(void *p, uint32_t flags) {
    ((Neopixels_t*)p)->OnDmaDone();
}

#if WS2812_DYNAMIC
void Neopixels_t::Init(int32_t ALedCnt) {
#else
void Neopixels_t::Init() {
#endif
    // GPIO and DMA
    PinSetupAlterFunc(Params->PGpio, Params->Pin, omPushPull, pudNone, Params->Af);
    Params->ISpi.Setup(boMSB, cpolIdleLow, cphaFirstEdge, 2500000, bitn8);
    Params->ISpi.Enable();
    Params->ISpi.EnableTxDma();

#if WS2812_DYNAMIC
    // Allocate memory
    Printf("LedCnt: %u\r", ALedCnt);
    LedCnt = ALedCnt;
    IBufSz = TOTAL_BYTE_CNT(LedCnt);
    Printf("TotalByteCnt: %u\r", IBufSz);
    IBitBuf = (uint8_t*)malloc(IBufSz);
    ClrBuf.resize(LedCnt);
    // Zero it all, to zero head and tail
    memset(IBitBuf, 0, IBufSz);
#endif

    // ==== DMA ====
    PDma = dmaStreamAlloc(Params->DmaID, IRQ_PRIO_LOW, NpxDmaDone, this);
    dmaStreamSetPeripheral(PDma, &Params->ISpi.PSpi->DR);
    dmaStreamSetMode      (PDma, Params->DmaMode);
}

static uint32_t ITable[256] = {
        0x244992,0x264992,0x344992,0x364992,0xA44992,0xA64992,0xB44992,0xB64992,0x244D92,0x264D92,0x344D92,0x364D92,0xA44D92,0xA64D92,0xB44D92,0xB64D92,
        0x246992,0x266992,0x346992,0x366992,0xA46992,0xA66992,0xB46992,0xB66992,0x246D92,0x266D92,0x346D92,0x366D92,0xA46D92,0xA66D92,0xB46D92,0xB66D92,
        0x244993,0x264993,0x344993,0x364993,0xA44993,0xA64993,0xB44993,0xB64993,0x244D93,0x264D93,0x344D93,0x364D93,0xA44D93,0xA64D93,0xB44D93,0xB64D93,
        0x246993,0x266993,0x346993,0x366993,0xA46993,0xA66993,0xB46993,0xB66993,0x246D93,0x266D93,0x346D93,0x366D93,0xA46D93,0xA66D93,0xB46D93,0xB66D93,
        0x24499A,0x26499A,0x34499A,0x36499A,0xA4499A,0xA6499A,0xB4499A,0xB6499A,0x244D9A,0x264D9A,0x344D9A,0x364D9A,0xA44D9A,0xA64D9A,0xB44D9A,0xB64D9A,
        0x24699A,0x26699A,0x34699A,0x36699A,0xA4699A,0xA6699A,0xB4699A,0xB6699A,0x246D9A,0x266D9A,0x346D9A,0x366D9A,0xA46D9A,0xA66D9A,0xB46D9A,0xB66D9A,
        0x24499B,0x26499B,0x34499B,0x36499B,0xA4499B,0xA6499B,0xB4499B,0xB6499B,0x244D9B,0x264D9B,0x344D9B,0x364D9B,0xA44D9B,0xA64D9B,0xB44D9B,0xB64D9B,
        0x24699B,0x26699B,0x34699B,0x36699B,0xA4699B,0xA6699B,0xB4699B,0xB6699B,0x246D9B,0x266D9B,0x346D9B,0x366D9B,0xA46D9B,0xA66D9B,0xB46D9B,0xB66D9B,
        0x2449D2,0x2649D2,0x3449D2,0x3649D2,0xA449D2,0xA649D2,0xB449D2,0xB649D2,0x244DD2,0x264DD2,0x344DD2,0x364DD2,0xA44DD2,0xA64DD2,0xB44DD2,0xB64DD2,
        0x2469D2,0x2669D2,0x3469D2,0x3669D2,0xA469D2,0xA669D2,0xB469D2,0xB669D2,0x246DD2,0x266DD2,0x346DD2,0x366DD2,0xA46DD2,0xA66DD2,0xB46DD2,0xB66DD2,
        0x2449D3,0x2649D3,0x3449D3,0x3649D3,0xA449D3,0xA649D3,0xB449D3,0xB649D3,0x244DD3,0x264DD3,0x344DD3,0x364DD3,0xA44DD3,0xA64DD3,0xB44DD3,0xB64DD3,
        0x2469D3,0x2669D3,0x3469D3,0x3669D3,0xA469D3,0xA669D3,0xB469D3,0xB669D3,0x246DD3,0x266DD3,0x346DD3,0x366DD3,0xA46DD3,0xA66DD3,0xB46DD3,0xB66DD3,
        0x2449DA,0x2649DA,0x3449DA,0x3649DA,0xA449DA,0xA649DA,0xB449DA,0xB649DA,0x244DDA,0x264DDA,0x344DDA,0x364DDA,0xA44DDA,0xA64DDA,0xB44DDA,0xB64DDA,
        0x2469DA,0x2669DA,0x3469DA,0x3669DA,0xA469DA,0xA669DA,0xB469DA,0xB669DA,0x246DDA,0x266DDA,0x346DDA,0x366DDA,0xA46DDA,0xA66DDA,0xB46DDA,0xB66DDA,
        0x2449DB,0x2649DB,0x3449DB,0x3649DB,0xA449DB,0xA649DB,0xB449DB,0xB649DB,0x244DDB,0x264DDB,0x344DDB,0x364DDB,0xA44DDB,0xA64DDB,0xB44DDB,0xB64DDB,
        0x2469DB,0x2669DB,0x3469DB,0x3669DB,0xA469DB,0xA669DB,0xB469DB,0xB669DB,0x246DDB,0x266DDB,0x346DDB,0x366DDB,0xA46DDB,0xA66DDB,0xB46DDB,0xB66DDB,
};

void Neopixels_t::SetCurrentColors() {
    TransmitDone = false;
    uint8_t *p = IBitBuf + (NPX_RST_BYTE_CNT / 2); // First and last words are zero to form reset
    // Fill bit buffer
    for(auto &Color : ClrBuf) {
        memcpy(p, &ITable[Color.G], 4);
        p += 3;
        memcpy(p, &ITable[Color.R], 4);
        p += 3;
        memcpy(p, &ITable[Color.B], 4);
        p += 3;
    }
//    Printf("%A\r", IBitBuf, IBufSz, ' ');
    // Start transmission
    dmaStreamDisable(PDma);
    dmaStreamSetMemory0(PDma, IBitBuf);
    dmaStreamSetTransactionSize(PDma, WS_IBufSz);
    dmaStreamSetMode(PDma, Params->DmaMode);
    dmaStreamEnable(PDma);
}

void Neopixels_t::OnDmaDone() {
    TransmitDone = true;
    if(OnTransmitEnd) OnTransmitEnd();
}
