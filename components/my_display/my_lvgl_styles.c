#include "esp_lvgl_port.h"

#include "my_display.h"
#include "my_lvgl_private.h"

// #include "my_config.h"

// 矩阵按钮控件中按键的样式
lv_style_t *my_lv_style_get_buttonm_item()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }

    lv_style_init(&style);
    lv_style_set_border_side(&style, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM));
    lv_style_set_border_width(&style, item_border_width);
    lv_style_set_border_color(&style, lv_color_hex(MY_COLOR_GREY));
    lv_style_set_border_post(&style, false);
    lv_style_set_border_opa(&style, LV_OPA_90);

    lv_style_set_shadow_width(&style, item_shadow_width);
    lv_style_set_shadow_spread(&style, item_shadow_spread);
    lv_style_set_shadow_offset_y(&style, 0);
    lv_style_set_shadow_offset_x(&style, 0);
    lv_style_set_shadow_color(&style, lv_color_hex(MY_COLOR_GREY));
    lv_style_set_shadow_opa(&style, LV_OPA_90);

    lv_style_set_radius(&style, 1);

    lv_style_set_outline_width(&style, item_outline_width);
    lv_style_set_outline_pad(&style, item_outline_pad);
    lv_style_set_outline_color(&style, lv_color_black());
    lv_style_set_outline_opa(&style, LV_OPA_90);

    lv_style_set_bg_color(&style, lv_color_white());
    lv_style_set_bg_opa(&style, 215);

    lv_style_set_text_font(&style, &lv_font_montserrat_12);
    lv_style_set_text_line_space(&style, item_text_line_space);
    lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

    lv_style_set_min_height(&style, item_obj_height - item_outline_width - item_outline_pad);

    return &style;
}

// 矩阵按钮控件中整体的样式
lv_style_t *my_lv_style_get_buttonm_main()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }
    lv_style_init(&style);
    lv_style_set_border_side(&style, LV_BORDER_SIDE_NONE);
    lv_style_set_border_width(&style, 0);
    lv_style_set_border_opa(&style, LV_OPA_0);

    lv_style_set_radius(&style, 0);

    lv_style_set_outline_width(&style, 0);
    lv_style_set_outline_color(&style, lv_color_black());
    lv_style_set_shadow_width(&style, 0);

    lv_style_set_pad_all(&style, main_pad_all);

    lv_style_set_pad_row(&style, main_pad_gap);
    lv_style_set_pad_column(&style, main_pad_gap);

    lv_style_set_bg_opa(&style, LV_OPA_0);
    return &style;
}

lv_style_t *my_lv_style_get_label_main()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }
    lv_style_init(&style);

    lv_style_set_text_font(&style, &lv_font_montserrat_12);
    lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_text_opa(&style, LV_OPA_60);
    lv_style_set_blend_mode(&style, LV_BLEND_MODE_NORMAL);
    lv_style_set_bg_opa(&style, LV_OPA_20);
    lv_style_set_bg_color(&style, lv_color_black());

    lv_style_set_outline_width(&style, 1);
    lv_style_set_outline_color(&style, lv_color_black());
    lv_style_set_outline_opa(&style, LV_OPA_20);

    lv_style_set_radius(&style, LV_RADIUS_CIRCLE);

    return &style;
}

lv_style_t *my_lv_style_get_label_enabled()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }
    lv_style_init(&style);
    // lv_style_copy(&style, &my_lv_style_get_label_main());

    lv_style_set_text_font(&style, &lv_font_montserrat_12);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_text_opa(&style, LV_OPA_100);
    lv_style_set_blend_mode(&style, LV_BLEND_MODE_NORMAL);
    lv_style_set_bg_opa(&style, LV_OPA_80);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_ORANGE));

    lv_style_set_outline_width(&style, 1);
    lv_style_set_outline_color(&style, lv_palette_main(LV_PALETTE_ORANGE));
    lv_style_set_outline_opa(&style, LV_OPA_80);

    lv_style_set_radius(&style, LV_RADIUS_CIRCLE);

    return &style;
}

lv_style_t *my_lv_style_get_led_main()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }
    lv_style_init(&style);

    lv_style_set_size(&style, 8, 8);
    lv_style_set_radius(&style, LV_RADIUS_CIRCLE);

    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_color_hex(0x404040));

    lv_style_set_shadow_width(&style, 1);
    lv_style_set_shadow_spread(&style, 0);
    lv_style_set_shadow_offset_x(&style, 1);
    lv_style_set_shadow_offset_y(&style, 1);

    return &style;
}

lv_style_t *my_lv_style_get_pop_window()
{
    static lv_style_t style;
    if (style.sentinel == LV_STYLE_SENTINEL_VALUE && style.prop_cnt > 1)
    {
        return &style;
    }
    lv_style_init(&style);

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_radius(&style, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&style, 5);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_outline_width(&style, 1);
    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_text_font(&style, &lv_font_montserrat_14);
    return &style;
}