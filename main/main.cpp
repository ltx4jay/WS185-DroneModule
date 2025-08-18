#include "MIC_Driver/MIC_Driver.h"
#include "ST77916.h"
#include "LVGL_Driver.h"
#include "core/lv_obj_pos.h"
#include "ui/ui.h"
#include "MIC_Driver.h"
#include "PWR_Key.h"
#include "QMI8658.h"
#include "PWR_Key.h"

#include <math.h>
#include <stdint.h> 
#include <cstddef>
#include <functional>
#include <type_traits>

#include "lvgl.h"
#include "ui/ui_events.h"
#include "widgets/lv_label.h"
#include "widgets/lv_slider.h"
#include "esp_sleep.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "globals.h"

extern "C" {
    void app_main(void);
}

static void defaultScreenService(void);
static void audioScreenService(void);

static lv_obj_t* gGimbal        = nullptr;
void (*screenLoop)(void) = nullptr;


// Default start screen, saved to NV-RAM
static uint8_t gStartScreen = 0;

void
switchScreen(bool confirmed, bool reset = false, int offset = 0)
{
    if (!confirmed) return;

    static struct
    {
        lv_obj_t* screenObj;
        lv_obj_t* image;
        uint32_t  duration;
        void (*init)(void);
    } screens[] = {
        {ui_Images, ui_HexCorp,     0, defaultScreenService},
        {ui_Images,     ui_CPU,     0, defaultScreenService},
        {ui_Images,     ui_BRK,     0, defaultScreenService},
        {ui_Images,    ui_Hive,     0, defaultScreenService},
        {ui_Images,    ui_HAL9000,  0, defaultScreenService},
        {ui_Images,  ui_Spiral, 35000, defaultScreenService},
        { ui_Audio,    nullptr,     0,   audioScreenService},
    };
    const auto cNumScreens = sizeof(screens) / sizeof(screens[0]);
    if (gStartScreen >= cNumScreens) gStartScreen = 0;

    static uint8_t nextScreen = 0;

    if (reset) {
        nextScreen = gStartScreen;
    } else {
        nextScreen = (nextScreen + cNumScreens + offset) % cNumScreens;
    }
    gStartScreen = nextScreen;

    // Hide all images except for the next one
    for (uint8_t i = 0; i < cNumScreens; i++) {
        if (screens[i].image == nullptr) continue;

        if (i == nextScreen) {
            lv_obj_clear_flag(screens[i].image, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(screens[i].image, LV_OBJ_FLAG_HIDDEN);
        }
    }

    auto& screen = screens[nextScreen];

    static lv_anim_t rotation;
    lv_anim_init(&rotation);
    
    gGimbal = nullptr;
    if (screen.duration > 0) {
        lv_anim_set_var(&rotation, screen.image);
        lv_anim_set_exec_cb(&rotation, [](void* img, int32_t v)
                            { lv_img_set_angle((lv_obj_t*)img, v); });
        lv_anim_set_values(&rotation, 0, 3600);
        lv_anim_set_time(&rotation, screen.duration);
        lv_anim_set_repeat_count(&rotation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&rotation);
    } else if (screen.image) {
        gGimbal = screen.image;
    }

    screen.init();

    lv_scr_load(screen.screenObj);
    Set_Backlight(gOpts.brightness);

    NVS_Store();
}

void resetScreen(bool confirmed)
{
    switchScreen(confirmed, true);
}

void refreshScreen()
{
    switchScreen(true, false, 0);
}

void nextScreen(bool confirmed)
{
    switchScreen(confirmed, false, 1);
}

void previousScreen(bool confirmed)
{
    switchScreen(confirmed, false, -1);
}

void vuMeter(int audio) {
    if (audio < 0) audio = -audio;

    // Gamma correct to stretch the low values
    audio = 100 * pow((float)audio / 4000.0, 0.5);

    static lv_obj_t* const vuMeter[9] = {ui_VUMeter0,
                                         ui_VUMeter1,
                                         ui_VUMeter2,
                                         ui_VUMeter3,
                                         ui_VUMeter4,
                                         ui_VUMeter5,
                                         ui_VUMeter6,
                                         ui_VUMeter7,
                                         ui_VUMeter8};

    static const float noise[9] = {0.5, 0.7, 0.8, 0.5, 0.0, 0.5, 0.8, 0.7, 0.5};

    for (int i = 0; i < 9; i++) {
        int32_t value = (1.0 - noise[i] * ((double) rand() / (RAND_MAX))) * audio;

        // If volume is too low, show a flat line
        if (audio < 25) value = 1;

        lv_slider_set_value(vuMeter[i], value, LV_ANIM_OFF);
        lv_slider_set_left_value(vuMeter[i], -value, LV_ANIM_OFF);
    }
}

void
NVS_Load()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    auto handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &err);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS for reading: %s", esp_err_to_name(err));
        return;
    }

    handle->get_item("startScreen", gStartScreen);
    handle->get_item("gimbalOn", gOpts.gimbalOn);
    handle->get_item("brightness", gOpts.brightness);
}

void
NVS_Store()
{
    esp_err_t err;
    auto handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &err);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS for writing: %s", esp_err_to_name(err));
        return;
    }

    handle->set_item("startScreen", gStartScreen);
    handle->set_item("gimbalOn", gOpts.gimbalOn);
    handle->set_item("brightness", gOpts.brightness);
}

void app_main(void)
{
    NVS_Load();
    PWR_Init();

    I2C_Init();
    LCD_Init();
    LVGL_Init();

    MIC_Init();
    QMI8658_Init();

    ui_init();
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);
    Set_Backlight(gOpts.brightness);

    // Show splash screen for 2 seconds
    lv_scr_load(ui_Splash);
    auto splashEndTime = xTaskGetTickCount() + pdMS_TO_TICKS(2000);

    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(1));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();

        auto now = xTaskGetTickCount();
        if (splashEndTime) {
            if (now < splashEndTime) continue;

            // Go to the last image that was displayed
            resetScreen(true);
            splashEndTime = 0;
        }

        if (screenLoop) screenLoop();
    }
}


void
fallAsleep(bool confirmed)
{
    if (!confirmed) {
        Set_Backlight(0);
        return;
    }

    PWR_Shutdown();
}

float saturate(float trigVal)
{
    if (trigVal > 1.0) return 1.0;
    if (trigVal < -1.0) return -1.0;

    return trigVal;
}


int16_t
degreesX10(float Ax, float Ay)
{
    // Since the accelaration units is g, assuming the device is at rest,
    // Ax is cosine and Ay is sine of the angle from true vertical
    auto sine   = saturate(Ay);
    auto cosine = saturate(Ax);

    // The cosine is very sensitive at the 0° and 180° points, use sine instead
    float angleX10 = 0.0;
    if (abs(cosine) > 0.7) {
        angleX10 = (asin(sine)) * 1800.0 / M_PI;
        if (cosine < 0) angleX10 = 1800 - angleX10;
    } else {
        angleX10 = (acos(cosine)) * 1800.0 / M_PI;
        if (sine < 0) angleX10 = 3600 - angleX10;
    }

    return angleX10;
}

void
defaultScreenLoop(void)
{
    PWR_Loop(nextScreen, fallAsleep, nullptr);

    if (gGimbal && gOpts.gimbalOn) {
        QMI8658_Loop();
        if (abs(Accel.z) < 0.300) {
            lv_img_set_angle(gGimbal, degreesX10(Accel.x, Accel.y));
        } else {
            lv_img_set_angle(gGimbal, 0);
        }
    }
}

void
defaultScreenService(void)
{
    screenLoop = defaultScreenLoop;
}

void
audioScreenLoop(void)
{
    // Touch_Loop();
    PWR_Loop(nextScreen, fallAsleep, nullptr);

    int sample;
    if (MIC_GetSample(sample)) vuMeter(sample);
}

void
audioScreenService(void)
{
    screenLoop = audioScreenLoop;
}

void
gotoNextImage_cb(lv_event_t *e)
{
    nextScreen(true);
}

void
gotoPreviousImage_cb(lv_event_t *e)
{
    previousScreen(true);

}
