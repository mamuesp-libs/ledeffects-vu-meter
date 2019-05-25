#include "mgos.h"
#include "led_master.h"

typedef struct {
    tools_rgb_data* color_bar;
    tools_rgb_data color_start;
    tools_rgb_data color_end;
    tools_rgb_data back_ground;
    tools_rgb_data black;
    audio_trigger_data* atd;
} vu_meter_data;

static vu_meter_data main_vmd;

static tools_rgb_data mgos_intern_vu_meter_calc_color(vu_meter_data* vmd, double percent)
{
    tools_rgb_data res;

    double r = (vmd->color_end.r * 1.0) + percent * (vmd->color_start.r - vmd->color_end.r);
    double g = (vmd->color_end.g * 1.0) + percent * (vmd->color_start.g - vmd->color_end.g);
    double b = (vmd->color_end.b * 1.0) + percent * (vmd->color_start.b - vmd->color_end.b);
    double a = (vmd->color_end.a * 1.0) + percent * (vmd->color_start.a - vmd->color_end.a);

    res.r = (int)r > 255.0 ? 255 : (int)r < 0.0 ? 0 : (int)r;
    res.g = (int)g > 255.0 ? 255 : (int)g < 0.0 ? 0 : (int)g;
    res.b = (int)b > 255.0 ? 255 : (int)b < 0.0 ? 0 : (int)b;
    res.a = (int)a > 255.0 ? 255 : (int)a < 0.0 ? 0 : (int)a;
    LOG(LL_VERBOSE_DEBUG, ("Perc:\t%.03f\tStart:\t0x%02X\t0x%02X\t0x%02X\tEnd:\t0x%02X\t0x%02X\t0x%02X\tRes:\t0x%02X\t0x%02X\t0x%02X", percent, vmd->color_start.r, vmd->color_start.g, vmd->color_start.b, vmd->color_end.r, vmd->color_end.g, vmd->color_end.b, res.r, res.g, res.b));

    return res;
}

static void mgos_intern_vu_meter_get_colors(mgos_rgbleds* leds, char* anim, vu_meter_data* vmd, bool random)
{
    if (random) {
        vmd->back_ground = tools_get_random_color(vmd->back_ground, &vmd->back_ground, 1, 1.0);
    } else {
        tools_rgb_data color = { 0, 0, 90, 0 };
        tools_rgb_data red = { 255, 0, 0, 0 };
        tools_rgb_data green = { 0, 255, 0, 0 };
        vmd->back_ground = color;
        vmd->color_start = green;
        vmd->color_end = red;
    }

    for (int y = 0; y < leds->panel_height; y++) {
        double percent = (y * 1.0) / (1.0 * (leds->panel_height - 1));
        vmd->color_bar[y] = mgos_intern_vu_meter_calc_color(vmd, percent);
    }
}

static bool mgos_intern_vu_meter_init(mgos_rgbleds* leds, vu_meter_data* vmd)
{
    memset(vmd, 0, sizeof(vu_meter_data));
    vmd->color_bar = calloc(leds->panel_height, sizeof(tools_rgb_data));
    
    vmd->atd = (audio_trigger_data*)leds->audio_data;
    leds->timeout = mgos_sys_config_get_ledeffects_vu_meter_timeout();
    leds->dim_all = mgos_sys_config_get_ledeffects_vu_meter_dim_all();
    // mgos_sys_config_get_ledeffects_vu_meter_color_start();
    // mgos_sys_config_get_ledeffects_vu_meter_color_end();
    // mgos_sys_config_get_ledeffects_vu_meter_threshold();

    mgos_intern_vu_meter_get_colors(leds, "vu_meter", vmd, false);

    mgos_universal_led_clear(leds);

    return true;
}

static void mgos_intern_vu_meter_exit(mgos_rgbleds* leds, vu_meter_data* vmd)
{
    free(vmd->color_bar);
    memset(vmd, 0, sizeof(vu_meter_data));
}

static void mgos_intern_vu_meter_loop(mgos_rgbleds* leds)
{
    static int counter = 0;
    vu_meter_data* curr_vmd = &main_vmd;
       
    if (curr_vmd->atd->is_noisy) {
        leds->pix_pos = 0;
    }

    int threshold = leds->panel_height - 1 - (int)round(leds->panel_height * curr_vmd->atd->level);
    threshold = threshold >= leds->panel_height ? leds->panel_height - 1 : threshold < 0 ? 0 : threshold;
    if (curr_vmd->atd->old_level >= curr_vmd->atd->norm_level) {
        if (threshold < ((leds->panel_height * 50) / 100)) {
            mgos_intern_vu_meter_get_colors(leds, "vu_meter", curr_vmd, true);
        }
    }
    mgos_universal_led_plot_all(leds, curr_vmd->back_ground);
    for (int y = 0; y < leds->panel_height; y++) {
        if (y > threshold) {
            // all columns show the same pixels ...
            for (int x = 0; x < leds->panel_width; x++) {
                mgos_universal_led_plot_pixel(leds, x, y, curr_vmd->color_bar[y], false);
            }
        }
    }
    mgos_universal_led_show(leds);

    if (counter == 0) {
        LOG(LL_VERBOSE_DEBUG, ("Level: %.03fV - norm: %ld, threshold: %d", curr_vmd->atd->last_level, curr_vmd->atd->norm_level, threshold));
    }
    counter++;
    counter %= 10;
}

void mgos_ledeffects_vu_meter(void* param, mgos_rgbleds_action action)
{
    static bool do_time = false;
    static uint32_t max_time = 0;
    uint32_t time = (mgos_uptime_micros() / 1000);
    mgos_rgbleds* leds = (mgos_rgbleds*)param;

    switch (action) {
    case MGOS_RGBLEDS_ACT_INIT:
        LOG(LL_INFO, ("mgos_ledeffects_vu_meter: called (init)"));
        mgos_intern_vu_meter_init(leds, &main_vmd);
        break;
    case MGOS_RGBLEDS_ACT_EXIT:
        LOG(LL_INFO, ("mgos_ledeffects_vu_meter: called (exit)"));
        mgos_intern_vu_meter_exit(leds, &main_vmd);
        break;
    case MGOS_RGBLEDS_ACT_LOOP:
        LOG(LL_VERBOSE_DEBUG, ("mgos_ledeffects_vu_meter: called (loop)"));
        mgos_intern_vu_meter_loop(leds);
        if (do_time) {
            time = (mgos_uptime_micros() /1000) - time;
            max_time = (time > max_time) ? time : max_time;
            LOG(LL_VERBOSE_DEBUG, ("VU meter loop duration: %d milliseconds, max: %d ...", time / 1000, max_time / 1000));
        }
        break;
    }
}

bool mgos_vu_meter_init(void) {
  LOG(LL_INFO, ("mgos_vu_meter_init ..."));
  ledmaster_add_effect("ANIM_VU_METER", mgos_ledeffects_vu_meter);
  return true;
}
