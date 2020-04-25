#include "board.h"
#include "led.h"
#include "Sequences.h"
#include "kl_lib.h"
#include "MsgQ.h"
#include "SimpleSensors.h"
#include "buttons.h"
#include "ws2812b.h"
#include "color.h"
#include "Effects.h"
#include "kl_adc.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);
static void EnterSleep();
bool IsEnteringSleep = false;

// Measure battery periodically
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
static void OnMeasurementDone();

// LEDs
ColorHSV_t hsv;
TmrKL_t TmrSave {TIME_MS2I(3600), evtIdTimeToSave, tktOneShot};
static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
Neopixels_t Leds{&NpxParams, BAND_CNT, BAND_SETUPS};
#endif

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V8);
    // PLL fed by HSI
    if(Clk.EnableHSI() == retvOk) {
        Clk.SetupFlashLatency(16);
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

#if BUTTONS_ENABLED
    SimpleSensors::Init();
#endif

    // ADC
    PinSetupOut(ADC_BAT_EN, omPushPull);
    PinSetHi(ADC_BAT_EN); // Enable it forever, as 200k produces ignorable current
    PinSetupAnalog(ADC_BAT_PIN);
    Adc.Init();
    TmrOneSecond.StartOrRestart();

    // Load and check color
//    Flash::Load((uint32_t*)&hsv, sizeof(ColorHSV_t));
    hsv.DWord32 = EE::Read32(0);
    if(hsv.H > 360) hsv.H = 120;
    hsv.S = 100;
    hsv.V = 100;

    // ==== Leds ====
    Leds.Init();
    // LED pwr pin
    PinSetupOut(NPX_PWR_PIN, omPushPull);
    PinSetHi(NPX_PWR_PIN);

    Eff::Init();
    Eff::SetColor(hsv.ToRGB());
    Eff::FadeIn();
//    Leds.SetAll(clGreen);
//    Leds.SetCurrentColors();

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
//                Printf("Btn %u\r", Msg.BtnEvtInfo.Type);
//                if(Msg.BtnEvtInfo.Type == beShortPress) {
//                    IsEnteringSleep = !IsEnteringSleep;
//                    if(IsEnteringSleep) Eff::FadeOut();
//                    else Eff::FadeIn();
//                }
                if(Msg.BtnEvtInfo.BtnID == 0) {
                    if(hsv.H < 360) hsv.H++;
                    else hsv.H = 0;
                }
                else if(Msg.BtnEvtInfo.BtnID == 1) {
                    if(hsv.H > 0) hsv.H--;
                    else hsv.H = 360;
                }
//                Printf("HSV %u; ", hsv.H);
                Eff::SetColor(hsv.ToRGB());
                // Prepare to save
                TmrSave.StartOrRestart();
                break;
#endif
            case evtIdTimeToSave:
                EE::Write32(0, hsv.DWord32);
                Eff::SetColor(clBlack);
                chThdSleepMilliseconds(450);
                Eff::SetColor(hsv.ToRGB());
                break;

            case evtIdFadeOutDone: /* EnterSleep(); */ break;
            case evtIdFadeInDone: break;

            case evtIdIsCharging: break;

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
    if(Adc.FirstConversion) Adc.FirstConversion = false;
    else {
        uint32_t VRef_adc = Adc.GetResult(ADC_VREFINT_CHNL);
        uint32_t Vadc = Adc.GetResult(BAT_CHNL);
        uint32_t Vmv = Adc.Adc2mV(Vadc, VRef_adc);
//        Printf("VrefAdc=%u; Vadc=%u; Vmv=%u\r", VRef_adc, Vadc, Vmv);
        uint32_t Battery_mV = Vmv * 2; // Resistor divider
//        Printf("Vbat=%u\r", Battery_mV);
        if(Battery_mV < 2800) {
            Printf("Discharged\r");
            EnterSleep();
        }
    }
}

void EnterSleep() {
    Printf("Entering sleep\r");
    PinSetLo(NPX_PWR_PIN);
    chThdSleepMilliseconds(45);
    chSysLock();
    Sleep::EnableWakeup1Pin();
    Sleep::EnterStandby();
    chSysUnlock();
}

#if 1 // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("Clr")) {
        uint8_t FClr[3];
        if(PCmd->GetArray<uint8_t>(FClr, 3) == retvOk) {
            Eff::SetColor(Color_t(FClr[0], FClr[1], FClr[2]));
            PShell->Ack(retvOk);
        }
        else PShell->Ack(retvCmdError);
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
