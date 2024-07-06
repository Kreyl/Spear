/*
 * tinwi.h
 *
 *  Created on: 13 июн. 2024 г.
 *      Author: layst
 */

#ifndef TINWI_H_
#define TINWI_H_

#include <inttypes.h>
#include "board.h"
#include "kl_lib.h"
#include "color.h"

#define PARAM_CNT   8UL

union Params {
    int32_t arr[PARAM_CNT];
    struct {
        int32_t smooth_min, smooth_max;
        int32_t hsv_v_min, hsv_v_max;
        int32_t hsv_h_min, hsv_h_max;
        int32_t tinwi_cnt;
        int32_t off_v;
    };
    int32_t GetSmooth() { return Random::Generate(smooth_min, smooth_max); }
    int32_t GetV()      { return Random::Generate(hsv_v_min, hsv_v_max); }
    int32_t GetH()      { return Random::Generate(hsv_h_min, hsv_h_max); }
};

namespace Tinwi {

void Init();
void FadeIn();
void FadeOut();
void ShowCharge(ColorHSV_t hsv);

extern Params *curr_params;
} // namespace

#endif /* TINWI_H_ */
