

#include "driver/adc_types_legacy.h"
#include "freertos/FreeRTOS.h"
#include "globals.h"

#include "PWR_Key.h"

#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#include "ui/ui.h"

#include "esp_log.h"

// Based on the provided test code. That means the battery monitor is on GPIO35, not GPIO8
// And that voltage divider is not 200k over 100k ohms.
const adc_channel_t cAdcChannel = ADC_CHANNEL_7;

static struct PowerState
{
    float      batteryVolts = 0.0f;
    lv_obj_t  *line       = nullptr;
    bool       isPressed  = false;
    TickType_t nextChange = 0;
    uint8_t    timeLeft;
} gPowerState;


static struct ADC {
    adc_oneshot_unit_handle_t unit;
    adc_cali_handle_t         calib;
} gADC;

static void
adcInit()
{
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_RC_FAST,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &gADC.unit));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(gADC.unit, cAdcChannel, &config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit_config.unit_id,
        .chan = cAdcChannel,
        .atten = config.atten,
        .bitwidth = config.bitwidth,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &gADC.calib));
}

void
senseBattery()
{
    int rawValue;
    adc_oneshot_read(gADC.unit, cAdcChannel, &rawValue);

    int calibratedValue;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(gADC.calib, rawValue, &calibratedValue));
    gPowerState.batteryVolts = (float)(calibratedValue * 3.0 / 1000.0) / 0.994500;
    
    ESP_LOGI("Battery", "ADC%d Channel[%d] Raw Data: %d  V=%.2f\n",
         ADC_UNIT_1 + 1, cAdcChannel, rawValue, gPowerState.batteryVolts);
}

void
powerScreenLoop(void)
{
    auto now = xTaskGetTickCount();
    if (now < gPowerState.nextChange) return;

    gPowerState.nextChange = now + pdMS_TO_TICKS(1000);

    senseBattery();

    int percent = (gPowerState.batteryVolts * 100) / 3.7;
    // if (percent > 100) percent = 100;
    lv_label_set_text_fmt(ui_Battery_Level, "%d%%", percent);

    if (gPowerState.isPressed && gPowerState.timeLeft > 0) {
        gPowerState.timeLeft--;
        
        lv_label_set_text_fmt(ui_Sleep_Timer, "00:%02d", gPowerState.timeLeft);
        lv_obj_set_style_text_font(ui_Sleep_Timer, &lv_font_montserrat_34, LV_PART_MAIN);

        lv_obj_add_flag(gPowerState.line, LV_OBJ_FLAG_HIDDEN);
    }
}

void
gotoPowerScreen_cb(lv_event_t *e)
{
    gPowerState.isPressed = false;
    gPowerState.nextChange = 0;

    screenLoop = powerScreenLoop;

    lv_scr_load(ui_Power);

    if (gPowerState.line == nullptr) {
        static lv_point_t line_points[] = {
            {270, 140},
            {340,  95}
        };

        lv_obj_t *line = lv_line_create(ui_Power);
        lv_line_set_points(line, line_points, 2);
        lv_obj_set_style_line_width(line, 4, LV_PART_MAIN);
        lv_obj_set_style_line_color(line, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        gPowerState.line = line;

        adcInit();
    }
}

void
exitPowerScreen_cb(lv_event_t *e)
{
    screenLoop = nullptr;

    lv_scr_load(ui_Images);
    refreshScreen();
}

void
gotoSleepButtonPressed_cb(lv_event_t *e)
{
    gPowerState.isPressed  = true;
    gPowerState.nextChange = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
    gPowerState.timeLeft   = 6;   // 5 seconds to sleep
}

void
gotoSleepButtonReleased_cb(lv_event_t *e)
{
    gPowerState.isPressed = false;
    if (gPowerState.timeLeft == 0) PWR_Shutdown();


    lv_label_set_text_fmt(ui_Sleep_Timer, "Long-press this button\nto wake up");
    lv_obj_set_style_text_font(ui_Sleep_Timer, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_clear_flag(gPowerState.line, LV_OBJ_FLAG_HIDDEN);
}