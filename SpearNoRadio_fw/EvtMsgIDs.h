/*
 * EvtMsgIDs.h
 *
 *  Created on: 21 ���. 2017 �.
 *      Author: Kreyl
 */

#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    evtIdShellCmd,
    evtIdEverySecond,
    evtIdAdcRslt,
    evtIdButtons,
    evtIdPwrOffTimeout,
    evtIdLedsDone,
};
