#include "board.h"
#include "kl_lib.h"
#include "uart.h"
#include "MsgQ.h"
#include "SimpleSensors.h"
#include "buttons.h"
#include "ws2812b.h"
#include "Flame.h"
#include "adcL151.h"
#include "Settings.h"
#include "battery_consts.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *pshell);
static void EnterSleep();
bool IsEnteringSleep = false;
bool ADC_first_conversion = true;
bool UsbIsConnected = false;
uint32_t battery_mV = 3700;

// Measure battery periodically
static TmrKL_t tmr_one_second {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
static void OnMeasurementDone();

// LEDs
static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
Neopixels_t leds{&NpxParams, BAND_CNT, BAND_SETUPS};
FlameSettings flame_setup;
#endif

static inline bool Btn1IsPressed() { return PinIsHi(BTN1_PIN); }

void EnterSleepNow() {
    Sleep::EnableWakeup1Pin(); // Btn0
    Sleep::EnterStandby();
}

// Will enter sleep if no need to wake up
static inline void CheckIfWakeupAtPwrOn() {
    // Get source of wakeup. Do not enter sleep if was not sleeping
    rccEnablePWRInterface(FALSE);
    if(PWR->CSR & PWR_CSR_WUF) { // Wakeup occured, check if this is button
        // Is it button?
        PinSetupInput(BTN1_PIN, pudPullDown);
        if(Btn1IsPressed()) {
            // Check if held pressed long enough
            for(uint32_t i=0; i<540000; i++)
                if(!Btn1IsPressed()) EnterSleepNow(); // Go sleep if btn released too fast
            // Btn was keeped in pressed state long enough, proceed with powerOn
        }
    }
}

void main(void) {
    CheckIfWakeupAtPwrOn();
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V8);
    // PLL fed by HSI
    if(Clk.EnableHSI() == retvOk) {
        Clk.SetupFlashLatency(11);
        Clk.SetupPLLSrc(pllSrcHSI16);
        Clk.SetupPLLDividers(pllMul4, pllDiv3);
        Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1);
        Clk.SwitchToPLL();
    }
    Clk.UpdateFreqValues();

    // === Init OS ===
    halInit();
    chSysInit();
    EvtQMain.Init();

    // ==== Init hardware ====
    Uart.Init();
    Printf("\r%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    // ==== Leds ====
    leds.Init();
    // LED pwr pin
    PinSetupOut(NPX_PWR_PIN, omPushPull);
    PinSetHi(NPX_PWR_PIN);

    flames.SetNewSettings(flame_setup);
    flames.Init();
    flames.FadeIn();

    // Wait until main button released
    while(Btn1IsPressed()) { chThdSleepMilliseconds(63); }
    SimpleSensors::Init(); // Buttons

    // ADC
    PinSetupOut(ADC_BAT_EN, omPushPull);
    PinSetHi(ADC_BAT_EN); // Enable it forever, as 200k produces ignorable current
    PinSetupAnalog(ADC_BAT_PIN);
    Adc.Init();
    tmr_one_second.StartOrRestart();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {

#if BUTTONS_ENABLED
            case evtIdButtons:
//                Printf("Btn %u %u\r", Msg.BtnEvtInfo.BtnID, Msg.BtnEvtInfo.Type);
                // Main button == BTN1
                if(Msg.BtnEvtInfo.BtnID == 0) {
                    if(Msg.BtnEvtInfo.Type == beLongPress) {
                        IsEnteringSleep = !IsEnteringSleep;
                        if(IsEnteringSleep) flames.FadeOut();
                        else flames.FadeIn();
                    }
                    else if(Msg.BtnEvtInfo.Type == beRelease) { // Show VBat
                        uint8_t percent = mV2PercentLiIon(battery_mV);
                        Printf("VBat: %umV; Percent: %u\r", battery_mV, percent);
                        ColorHSV_t hsv;
                        if     (percent <= 20) hsv = {0,   100, 100};
                        else if(percent <  60) hsv = {30,  100, 100};
                        else if(percent <  90) hsv = {120, 100, 100};
                        else                   hsv = {240, 100, 100};
                        flames.ShowCharge(hsv);
                    }
                }
                break;
#endif
            case evtIdLedsDone:
                if(!UsbIsConnected) EnterSleep();
                break;

            case evtIdEverySecond: Adc.StartMeasurement(); break;
            case evtIdAdcRslt: OnMeasurementDone(); break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

void ProcessIsCharging(PinSnsState_t *PState, uint32_t Len) {
    if(*PState == pssLo) {
        Printf("Charging\r");
        EnterSleep();
    }
}

void OnMeasurementDone() {
//    Printf("AdcDone\r");
    if(ADC_first_conversion) ADC_first_conversion = false;
    else {
        uint32_t VRef_adc = Adc.GetResultMedian(ADC_VREFINT_CHNL);
        uint32_t Vadc = Adc.GetResultMedian(BAT_CHNL);
        uint32_t Vmv = Adc.Adc2mV(Vadc, VRef_adc);
//        Printf("VrefAdc=%u; Vadc=%u; Vmv=%u\r", VRef_adc, Vadc, Vmv);
        battery_mV = Vmv * 2; // Resistor divider
//        Printf("Vbat=%u\r", Battery_mV);
        if(battery_mV < 3300) {
            Printf("Discharged: Vbat=%u mV\r", battery_mV);
            EnterSleep();
        }
    }
}

void EnterSleep() {
    Printf("Entering sleep\r");
    PinSetLo(NPX_PWR_PIN);
    chThdSleepMilliseconds(45);
    chSysDisable();
    EnterSleepNow();
}

#if 1 // ================= Command processing ====================
uint8_t GetSetup(Cmd_t *PCmd, FlameSettings &FSett) {
    if(     PCmd->GetNext<uint8_t>(&FSett.Core.Sz) == retvOk and
            PCmd->GetNext<uint16_t>(&FSett.Core.ClrHMin) == retvOk and
            PCmd->GetNext<uint16_t>(&FSett.Core.ClrHMax) == retvOk and
            PCmd->GetNext<uint8_t>(&FSett.Core.ClrV) == retvOk and
            PCmd->GetNext<uint8_t>(&FSett.Sparks.Cnt) == retvOk and
            PCmd->GetNext<uint8_t>(&FSett.Sparks.TailLen) == retvOk and
            PCmd->GetNext<uint16_t>(&FSett.Sparks.ClrHMin) == retvOk and
            PCmd->GetNext<uint16_t>(&FSett.Sparks.ClrHMax) == retvOk and
            PCmd->GetNext<uint8_t>(&FSett.Sparks.ClrV) == retvOk and
            PCmd->GetNext<uint16_t>(&FSett.Sparks.DelayBeforeRestart) == retvOk and
            PCmd->GetNext<int16_t>(&FSett.Sparks.AccMin) == retvOk and
            PCmd->GetNext<int16_t>(&FSett.Sparks.AccMax) == retvOk and
            PCmd->GetNext<int16_t>(&FSett.Sparks.StartDelayMin) == retvOk and
            PCmd->GetNext<int16_t>(&FSett.Sparks.StartDelayMax) == retvOk and
            PCmd->GetNext<uint8_t>(&FSett.Sparks.Mode) == retvOk
        ) return retvOk;
    else return retvFail;
}

void OnCmd(Shell_t *pshell) {
	Cmd_t *pcmd = &pshell->Cmd;
    if(pcmd->NameIs("Ping")) pshell->Ack(retvOk);
    else if(pcmd->NameIs("Version")) pshell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(pcmd->NameIs("SetAll")) {
        Color_t clr;
        if(pcmd->GetParams<uint8_t>(3, &clr.R, &clr.G, &clr.B) == retvOk) {
            leds.SetAll(clr);
            leds.SetCurrentColors();
            pshell->Ack(retvOk);
        }
        else pshell->Ack(retvBadValue);
    }

    else if(pcmd->NameIs("Setup")) {
        FlameSettings FSett;
        if(GetSetup(pcmd, FSett) == retvOk) {
            flames.SetNewSettings(FSett);
            pshell->Ack(retvOk);
        }
        else pshell->Ack(retvBadValue);
    }

    else pshell->Ack(retvCmdUnknown);
}
#endif
