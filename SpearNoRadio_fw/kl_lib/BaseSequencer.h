/*
 * ChunkTypes.h
 *
 *  Created on: 08 Jan 2015
 *      Author: Kreyl
 */

#ifndef CHUNK_TYPES_H_
#define CHUNK_TYPES_H_

#include "color.h"
#include "kl_lib.h"

enum class Chunk {Setup, Wait, Goto, End, Repeat};

// ==== Different types of chunks ====
struct BaseChunk_t { //  Everyone must contain this.
    Chunk chunk_sort;
    union {
        uint32_t value;
        uint32_t volume;
        uint32_t time_ms;
        uint32_t smooth_value;
        uint32_t chunk_to_jump_to;
        int32_t repeat_cnt;
    };
};

// RGB LED chunk
struct LedRGBChunk_t : public BaseChunk_t {
    Color_t color;
};

// HSV LED chunk
struct LedHSVChunk_t : public BaseChunk_t {
    ColorHSV_t color;
};

// LED Smooth
struct LedSmoothChunk_t : public BaseChunk_t {
    int32_t brightness;
};

// Beeper
struct BeepChunk_t : public BaseChunk_t {
    uint32_t freq_Hz;
    uint32_t freq_smooth = 0;
};


#if 1 // ====================== Base sequencer class ===========================
void TmrKLCallback(void *p);

template <class TChunk>
class BaseSequencer_t : private IrqHandler_t {
protected:
    virtual_timer_t itmr;
    const TChunk *pstart_chunk = nullptr, *pcurrent_chunk = nullptr, *pnext_chunk = nullptr;
    int32_t repeat_counter = -1;
    virtual void SwitchOff() = 0;
    virtual bool AdjustAndCheckIfGotoNextChunk() = 0;
    void SetupDelay(uint32_t ms) { chVTSetI(&itmr, TIME_MS2I(ms), TmrKLCallback, this); }

    // Process sequence
    void IIrqHandlerI() {
        if(chVTIsArmedI(&itmr)) chVTResetI(&itmr);  // Reset timer
        while(true) {   // Process the sequence
            switch(pcurrent_chunk->chunk_sort) {
                case Chunk::Setup: // setup now and exit if ready
                    if(AdjustAndCheckIfGotoNextChunk()) pcurrent_chunk++;
                    else return;
                    break;

                case Chunk::Wait: { // Start timer, pointing to next chunk
                        uint32_t Delay = pcurrent_chunk->time_ms;
                        pcurrent_chunk++;
                        if(Delay != 0) {
                            SetupDelay(Delay);
                            return;
                        }
                    }
                    break;

                case Chunk::Repeat:
                    if(repeat_counter == -1) repeat_counter = pcurrent_chunk->repeat_cnt;
                    if(repeat_counter == 0) {    // All was repeated, goto next
                        repeat_counter = -1;     // reset counter
                        pcurrent_chunk++;
                    }
                    else {  // repeating in progress
                        pcurrent_chunk = pstart_chunk;  // Always from beginning
                        repeat_counter--;
                    }
                    break;

                case Chunk::Goto:
                    pcurrent_chunk = pstart_chunk + pcurrent_chunk->chunk_to_jump_to;
                    if(end_callback) end_callback(this);
//                    if(ievt_msg.evt_id != EvtId::None) EvtQMain.SendNowOrExitI(ievt_msg);
                    SetupDelay(1);
                    return;
                    break;

                case Chunk::End:
                    if(end_callback) end_callback(this);
//                    if(ievt_msg.evt_id != EvtId::None) EvtQMain.SendNowOrExitI(ievt_msg);
                    if(pnext_chunk == nullptr) { // There is nothing next
                        pstart_chunk = nullptr;
                        pcurrent_chunk = nullptr;
                        return;
                    }
                    else { // There is something next
                        repeat_counter = -1;
                        pstart_chunk = pnext_chunk;
                        pcurrent_chunk = pnext_chunk;
                        pnext_chunk = nullptr;
                    }
                    break;
            } // switch
        } // while
    } // IProcessSequenceI
public:
    ftVoidPVoid end_callback = nullptr;
//    void SetupSeqEndEvt(EvtMsg_t AEvtMsg) { ievt_msg = AEvtMsg; }

    void StartOrRestartI(const TChunk *PChunk) {
        repeat_counter = -1;
        pstart_chunk = PChunk;   // Save first chunk
        pcurrent_chunk = PChunk;
        pnext_chunk = nullptr;
        IIrqHandlerI();
    }

    void StartOrRestart(const TChunk *PChunk) {
        chSysLock();
        StartOrRestartI(PChunk);
        chSysUnlock();
    }

    void StartOrContinue(const TChunk *PChunk) {
        if(PChunk == pstart_chunk) return; // Same sequence
        else StartOrRestart(PChunk);
    }

    void StopI() {
        if(pstart_chunk != nullptr) {
            if(chVTIsArmedI(&itmr)) chVTResetI(&itmr);
            pstart_chunk = nullptr;
            pcurrent_chunk = nullptr;
            pnext_chunk = nullptr;
        }
        SwitchOff();
    }

    void Stop() {
        chSysLock();
        StopI();
        chSysUnlock();
    }

    const TChunk* GetCurrentSequence() { return pstart_chunk; }

    // Next sequence will be started after current ends
    void SetNextSequenceI(const TChunk *PChunk) {
        if(IsIdle() and PChunk != nullptr) StartOrRestartI(PChunk);
        else pnext_chunk = PChunk;
    }

    bool IsIdle() { return (pstart_chunk == nullptr and pcurrent_chunk == nullptr); }
};
#endif

#endif // CHUNK_TYPES_H_
