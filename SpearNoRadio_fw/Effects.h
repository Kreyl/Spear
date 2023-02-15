/*
 * Effects.h
 *
 *  Created on: 5 ???. 2019 ?.
 *      Author: Kreyl
 */

#pragma once

#include "color.h"

#define START_CLR   clYellow

namespace Eff {
void Init();

//void SetColor(Color_t AClr);
void SetBackColor(Color_t AClr);

void FadeIn();
void FadeOut();
}
