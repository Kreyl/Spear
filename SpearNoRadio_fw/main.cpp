#include "board.h"
#include "led.h"
#include "Sequences.h"
#include "kl_lib.h"
#include "MsgQ.h"
#include "SimpleSensors.h"
#include "buttons.h"
#include "ws2812b.h"
#include "color.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

static TmrKL_t TmrOneSecond {TIME_MS2I(180), evtIdEverySecond, tktPeriodic};

// LEDs
static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
static Neopixels_t Leds{&NpxParams};
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

    // ==== Leds ====
    Leds.Init();
    Leds.SetAll(clGreen);
    // Pwr pin
    PinSetupOut(NPX_PWR_PIN, omPushPull);
    PinSetHi(NPX_PWR_PIN);

    TmrOneSecond.StartOrRestart();

    // Main cycle
    ITask();
}

ColorHSV_t ClrHsv(0, 100, 100);

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {

#if BUTTONS_ENABLED
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.Type);
//                if(Msg.BtnEvtInfo.Type == beShortPress) {
//                }
//                else if(Msg.BtnEvtInfo.Type == beLongCombo and Msg.BtnEvtInfo.BtnCnt == 3) {
//                    Printf("Combo\r");
//                }
                break;
#endif

            case evtIdEverySecond:
                Leds.SetAll(ClrHsv.ToRGB());
                if(++ClrHsv.H > 360) ClrHsv.H=0;
                Leds.SetCurrentColors();
                break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

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

    else PShell->Ack(retvCmdUnknown);
}
#endif
