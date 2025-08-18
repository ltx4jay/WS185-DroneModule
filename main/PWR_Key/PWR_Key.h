#pragma once

#include <functional>

using PWR_Callback_t = std::function<void(bool released)>;

void PWR_Init(void);
void PWR_Loop(PWR_Callback_t clicked, PWR_Callback_t longPress, PWR_Callback_t veryLongPress);
void PWR_WaitNotPressed(void);
void PWR_Shutdown();