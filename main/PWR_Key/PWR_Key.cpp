#include "PWR_Key.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "ST77916.h"


const gpio_num_t PWR_KEY_Input_PIN = GPIO_NUM_6;
const gpio_num_t PWR_Control_PIN   = GPIO_NUM_7;

const bool PRESSED = false;   // Negative logic

void
PWR_Loop(PWR_Callback_t clicked, PWR_Callback_t longPress, PWR_Callback_t veryLongPress)
{
    static bool       lastValue  = !PRESSED;
    static TickType_t lastChange = 0;
    const TickType_t  LONGPRESS     = pdMS_TO_TICKS(3000);
    const TickType_t  VERLONGPRESS  = pdMS_TO_TICKS(10000);

    static enum {START, SHORT, LONG, VERYLONG} state = START;

    static PWR_Callback_t callback = nullptr;

    auto now = xTaskGetTickCount();
    if (gpio_get_level(PWR_KEY_Input_PIN) != lastValue) {
        lastChange = now;
        state = START;
        if (lastValue == PRESSED) {
            // The button was just released
            if (callback) callback(true);
            callback = nullptr;
        }
        lastValue = !lastValue;
    } else {
        if (lastValue == PRESSED) {

            // Decide what we'll do based on how long the button is pressed.
            if (now >= lastChange + VERLONGPRESS) {
                if (veryLongPress && state != VERYLONG) {
                    state = VERYLONG;
                    callback = veryLongPress;
                    callback(false);
                }
            } else if (now >= lastChange + LONGPRESS) {
                if (longPress && state != LONG) {
                    state = LONG;
                    callback = longPress;
                    callback(false);
                }
            } else {
                if (state != SHORT) {
                    state = SHORT;
                    callback = clicked;
                    callback(false);
                }
            }
            
        }
    }
}

void
PWR_WaitNotPressed()
{
    while (1) {
        if (gpio_get_level(PWR_KEY_Input_PIN) != PRESSED) return;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


static void configure_GPIO(gpio_num_t pin, gpio_mode_t Mode, gpio_pull_mode_t pull = GPIO_FLOATING)
{
    gpio_reset_pin(pin);                                     
    gpio_set_direction(pin, Mode);    
    gpio_set_pull_mode(pin, pull);                      
}

void
PWR_Init(void)
{
    configure_GPIO(PWR_KEY_Input_PIN, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY);
    configure_GPIO(PWR_Control_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PWR_Control_PIN, true);

    PWR_WaitNotPressed();
}

void
PWR_Shutdown()
{
    esp_sleep_enable_ext0_wakeup(PWR_KEY_Input_PIN, 0);
    gpio_set_level(PWR_Control_PIN, false);
    esp_deep_sleep_start();
}
