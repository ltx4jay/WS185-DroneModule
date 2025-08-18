
#include "globals.h"

#include "ui/ui.h"
#include "LCD_Driver/ST77916.h"

struct Options gOpts;


void
gotoOptionsScreen_cb(lv_event_t *e)
{
    screenLoop = nullptr;

    lv_label_set_text(ui_Option_Label_1, "Gimbal Mode");
    if (gOpts.gimbalOn) {
        lv_obj_add_state(ui_Option_1, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_Option_1, LV_STATE_CHECKED);
    }

    lv_slider_set_value(ui_Brightness_Slider, gOpts.brightness, LV_ANIM_OFF);

    lv_obj_add_flag(ui_Option_Container_3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Option_Container_4, LV_OBJ_FLAG_HIDDEN);

    lv_scr_load(ui_Options);
    Set_Backlight(gOpts.brightness);
}

void
option1_ON_cb(lv_event_t *e)
{
    gOpts.gimbalOn = true;
}

void
option1_OFF_cb(lv_event_t *e)
{
    gOpts.gimbalOn = false;
}

void
setBrightness_cb(lv_event_t *e)
{
    gOpts.brightness = lv_slider_get_value(ui_Brightness_Slider);
    Set_Backlight(gOpts.brightness);
}

void
option3_ON_cb(lv_event_t *e)
{
}

void
option3_OFF_cb(lv_event_t *e)
{
}

void
option4_ON_cb(lv_event_t *e)
{
}

void
option4_OFF_cb(lv_event_t *e)
{
}

void
exitOptionsScreen_cb(lv_event_t *e)
{
    NVS_Store();
    
    lv_scr_load(ui_Images);
    refreshScreen();
}
