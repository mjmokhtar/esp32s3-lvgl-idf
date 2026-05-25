/*
 * fuel_hmi.c  v2
 * Fuel Monitoring HMI — GE U18C Locomotive
 * Layout: Left panel (fuel) + Center panel (flow + chart)
 * Single CAN bus — Eurosens Dominator J1939 250kbps
 */

#include "fuel_hmi.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════
 *  Widget handles
 * ════════════════════════════════════════════════════════════ */
static struct {
    /* Top bar */
    lv_obj_t *lbl_time;
    lv_obj_t *dot_can1;

    /* Left — fuel */
    lv_obj_t *arc_fuel;
    lv_obj_t *lbl_fuel_pct;
    lv_obj_t *bar_fuel_vol;
    lv_obj_t *lbl_fuel_vol;
    lv_obj_t *lbl_fuel_temp;
    lv_obj_t *lbl_can1_status;

    /* Center — flow cards */
    lv_obj_t *lbl_flow_supply;
    lv_obj_t *lbl_flow_return;
    lv_obj_t *lbl_net_flow;
    lv_obj_t *lbl_total_used;
    lv_obj_t *lbl_range;

    /* Center — chart */
    lv_obj_t         *chart;
    lv_chart_series_t *ser_supply;
    lv_chart_series_t *ser_return;
    lv_chart_series_t *ser_net;

    /* Alarms */
    lv_obj_t *alarm_theft;
    lv_obj_t *alarm_temp;
    lv_obj_t *alarm_can;
    lv_obj_t *alarm_ok;

    /* Bottom */
    lv_obj_t *lbl_uptime;
} g_w;

/* ════════════════════════════════════════════════════════════
 *  Styles
 * ════════════════════════════════════════════════════════════ */
static lv_style_t s_screen, s_topbar, s_botbar, s_panel_left,
                  s_card, s_card_alarm,
                  s_lbl_hint, s_lbl_dim, s_lbl_pri,
                  s_lbl_accent, s_lbl_blue, s_lbl_green, s_lbl_red;

static void styles_init(void)
{
    /* Screen */
    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, HMI_COL_BG);
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_pad_all(&s_screen, 0);

    /* Top bar */
    lv_style_init(&s_topbar);
    lv_style_set_bg_color(&s_topbar, lv_color_make(17,17,17));
    lv_style_set_bg_opa(&s_topbar, LV_OPA_COVER);
    lv_style_set_border_side(&s_topbar, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&s_topbar, HMI_COL_ACCENT);
    lv_style_set_border_width(&s_topbar, 1);
    lv_style_set_pad_hor(&s_topbar, 16);
    lv_style_set_pad_ver(&s_topbar, 0);

    /* Bottom bar */
    lv_style_init(&s_botbar);
    lv_style_set_bg_color(&s_botbar, lv_color_make(13,13,13));
    lv_style_set_bg_opa(&s_botbar, LV_OPA_COVER);
    lv_style_set_border_side(&s_botbar, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&s_botbar, lv_color_make(34,34,34));
    lv_style_set_border_width(&s_botbar, 1);
    lv_style_set_pad_hor(&s_botbar, 16);
    lv_style_set_pad_ver(&s_botbar, 0);

    /* Left panel */
    lv_style_init(&s_panel_left);
    lv_style_set_bg_color(&s_panel_left, HMI_COL_PANEL);
    lv_style_set_bg_opa(&s_panel_left, LV_OPA_COVER);
    lv_style_set_border_side(&s_panel_left, LV_BORDER_SIDE_RIGHT);
    lv_style_set_border_color(&s_panel_left, lv_color_make(51,51,51));
    lv_style_set_border_width(&s_panel_left, 1);
    lv_style_set_pad_all(&s_panel_left, 12);

    /* Cards */
    lv_style_init(&s_card);
    lv_style_set_bg_color(&s_card, HMI_COL_CARD);
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_border_color(&s_card, HMI_COL_BORDER);
    lv_style_set_border_width(&s_card, 1);
    lv_style_set_radius(&s_card, 2);
    lv_style_set_pad_all(&s_card, 8);

    lv_style_init(&s_card_alarm);
    lv_style_set_bg_color(&s_card_alarm, lv_color_make(26,10,0));
    lv_style_set_bg_opa(&s_card_alarm, LV_OPA_COVER);
    lv_style_set_border_color(&s_card_alarm, lv_color_make(80,40,0));
    lv_style_set_border_width(&s_card_alarm, 1);
    lv_style_set_radius(&s_card_alarm, 2);
    lv_style_set_pad_all(&s_card_alarm, 8);

    /* Text */
    lv_style_init(&s_lbl_hint);
    lv_style_set_text_color(&s_lbl_hint, HMI_COL_TEXT_HINT);
    lv_style_set_text_font(&s_lbl_hint, &lv_font_montserrat_10);

    lv_style_init(&s_lbl_dim);
    lv_style_set_text_color(&s_lbl_dim, HMI_COL_TEXT_DIM);
    lv_style_set_text_font(&s_lbl_dim, &lv_font_montserrat_10);

    lv_style_init(&s_lbl_pri);
    lv_style_set_text_color(&s_lbl_pri, HMI_COL_TEXT_PRI);
    lv_style_set_text_font(&s_lbl_pri, &lv_font_montserrat_14);

    lv_style_init(&s_lbl_accent);
    lv_style_set_text_color(&s_lbl_accent, HMI_COL_ACCENT);
    lv_style_set_text_font(&s_lbl_accent, &lv_font_montserrat_22);

    lv_style_init(&s_lbl_blue);
    lv_style_set_text_color(&s_lbl_blue, HMI_COL_BLUE);
    lv_style_set_text_font(&s_lbl_blue, &lv_font_montserrat_22);

    lv_style_init(&s_lbl_green);
    lv_style_set_text_color(&s_lbl_green, HMI_COL_GREEN);
    lv_style_set_text_font(&s_lbl_green, &lv_font_montserrat_22);

    lv_style_init(&s_lbl_red);
    lv_style_set_text_color(&s_lbl_red, HMI_COL_RED);
    lv_style_set_text_font(&s_lbl_red, &lv_font_montserrat_22);
}

/* ════════════════════════════════════════════════════════════
 *  Top bar
 * ════════════════════════════════════════════════════════════ */
static void build_topbar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_add_style(bar, &s_topbar, 0);
    lv_obj_set_size(bar, HMI_SCREEN_W, HMI_TOPBAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, LV_SYMBOL_CHARGE " FUEL MONITORING SYSTEM - GE U18C LOCOMOTIVE");
    lv_obj_set_style_text_color(title, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);

    /* Right row */
    lv_obj_t *right = lv_obj_create(bar);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_layout(right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 20, 0);

    g_w.dot_can1 = lv_label_create(right);
    lv_label_set_text(g_w.dot_can1, LV_SYMBOL_OK " CAN BUS OK");
    lv_obj_set_style_text_color(g_w.dot_can1, HMI_COL_GREEN, 0);
    lv_obj_set_style_text_font(g_w.dot_can1, &lv_font_montserrat_10, 0);

    lv_obj_t *lbl_tc = lv_label_create(right);
    lv_label_set_text(lbl_tc, "TIME:");
    lv_obj_add_style(lbl_tc, &s_lbl_dim, 0);

    g_w.lbl_time = lv_label_create(right);
    lv_label_set_text(g_w.lbl_time, "--:--:--");
    lv_obj_add_style(g_w.lbl_time, &s_lbl_pri, 0);
}

/* ════════════════════════════════════════════════════════════
 *  Left panel — fuel gauge + volume bar + temp + CAN status
 * ════════════════════════════════════════════════════════════ */
static void build_left_panel(lv_obj_t *content)
{
    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_add_style(panel, &s_panel_left, 0);
    lv_obj_set_size(panel, HMI_LEFT_W, HMI_CONTENT_H);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 10, 0);

    /* Section label */
    lv_obj_t *sec = lv_label_create(panel);
    lv_label_set_text(sec, "FUEL LEVEL");
    lv_obj_set_style_text_color(sec, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(sec, &lv_font_montserrat_10, 0);
    lv_obj_set_width(sec, LV_PCT(100));

    /* Arc */
    g_w.arc_fuel = lv_arc_create(panel);
    lv_obj_set_size(g_w.arc_fuel, 95, 95);
    lv_arc_set_rotation(g_w.arc_fuel, 135);
    lv_arc_set_bg_angles(g_w.arc_fuel, 0, 270);
    lv_arc_set_range(g_w.arc_fuel, 0, 100);
    lv_arc_set_value(g_w.arc_fuel, 68);
    lv_obj_set_style_arc_color(g_w.arc_fuel, lv_color_make(30,30,30), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_w.arc_fuel, HMI_COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_w.arc_fuel, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_w.arc_fuel, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(g_w.arc_fuel, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(g_w.arc_fuel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_w.arc_fuel, 0, 0);
    lv_obj_clear_flag(g_w.arc_fuel, LV_OBJ_FLAG_CLICKABLE);

    /* Pct label */
    g_w.lbl_fuel_pct = lv_label_create(panel);
    lv_label_set_text(g_w.lbl_fuel_pct, "68%");
    lv_obj_set_style_text_color(g_w.lbl_fuel_pct, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(g_w.lbl_fuel_pct, &lv_font_montserrat_28, 0);
    lv_obj_align_to(g_w.lbl_fuel_pct, g_w.arc_fuel, LV_ALIGN_CENTER, 0, 10);

    /* Volume section */
    lv_obj_t *vol_wrap = lv_obj_create(panel);
    lv_obj_set_width(vol_wrap, LV_PCT(100));
    lv_obj_set_height(vol_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(vol_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_wrap, 0, 0);
    lv_obj_set_style_pad_all(vol_wrap, 0, 0);
    lv_obj_set_layout(vol_wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(vol_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(vol_wrap, 4, 0);

    lv_obj_t *vol_hdr = lv_obj_create(vol_wrap);
    lv_obj_set_size(vol_hdr, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(vol_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_hdr, 0, 0);
    lv_obj_set_style_pad_all(vol_hdr, 0, 0);
    lv_obj_set_layout(vol_hdr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(vol_hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vol_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *vc = lv_label_create(vol_hdr);
    lv_label_set_text(vc, "VOLUME");
    lv_obj_add_style(vc, &s_lbl_hint, 0);
    g_w.lbl_fuel_vol = lv_label_create(vol_hdr);
    lv_label_set_text(g_w.lbl_fuel_vol, "2,856 L");
    lv_obj_set_style_text_color(g_w.lbl_fuel_vol, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(g_w.lbl_fuel_vol, &lv_font_montserrat_14, 0);

    g_w.bar_fuel_vol = lv_bar_create(vol_wrap);
    lv_obj_set_size(g_w.bar_fuel_vol, LV_PCT(100), 16);
    lv_bar_set_range(g_w.bar_fuel_vol, 0, 4200);
    lv_bar_set_value(g_w.bar_fuel_vol, 2856, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_w.bar_fuel_vol, lv_color_make(26,26,26), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_w.bar_fuel_vol, HMI_COL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_w.bar_fuel_vol, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(g_w.bar_fuel_vol, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_w.bar_fuel_vol, HMI_COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_w.bar_fuel_vol, 2, LV_PART_INDICATOR);

    lv_obj_t *vol_lim = lv_obj_create(vol_wrap);
    lv_obj_set_size(vol_lim, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(vol_lim, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_lim, 0, 0);
    lv_obj_set_style_pad_all(vol_lim, 0, 0);
    lv_obj_set_layout(vol_lim, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(vol_lim, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vol_lim, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *l0 = lv_label_create(vol_lim); lv_label_set_text(l0, "0 L");
    lv_obj_add_style(l0, &s_lbl_hint, 0);
    lv_obj_t *lm = lv_label_create(vol_lim); lv_label_set_text(lm, "4,200 L MAX");
    lv_obj_add_style(lm, &s_lbl_hint, 0);

    /* Temp card */
    lv_obj_t *tc = lv_obj_create(panel);
    lv_obj_set_size(tc, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(tc, &s_card, 0);
    lv_obj_set_layout(tc, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tc, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tc, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *tc_cap = lv_label_create(tc);
    lv_label_set_text(tc_cap, "TEMP");
    lv_obj_add_style(tc_cap, &s_lbl_hint, 0);
    lv_obj_t *tc_right = lv_obj_create(tc);
    lv_obj_set_size(tc_right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(tc_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tc_right, 0, 0);
    lv_obj_set_style_pad_all(tc_right, 0, 0);
    lv_obj_set_layout(tc_right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tc_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tc_right, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    g_w.lbl_fuel_temp = lv_label_create(tc_right);
    lv_label_set_text(g_w.lbl_fuel_temp, "32.0°C");
    lv_obj_set_style_text_color(g_w.lbl_fuel_temp, HMI_COL_RED, 0);
    lv_obj_set_style_text_font(g_w.lbl_fuel_temp, &lv_font_montserrat_18, 0);
    lv_obj_t *tc_sub = lv_label_create(tc_right);
    lv_label_set_text(tc_sub, "FUEL TEMP");
    lv_obj_add_style(tc_sub, &s_lbl_hint, 0);

    /* CAN status card */
    lv_obj_t *can_card = lv_obj_create(panel);
    lv_obj_set_size(can_card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(can_card, &s_card, 0);
    lv_obj_set_layout(can_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(can_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(can_card, 4, 0);

    lv_obj_t *can_t = lv_label_create(can_card);
    lv_label_set_text(can_t, "CAN BUS STATUS");
    lv_obj_add_style(can_t, &s_lbl_hint, 0);

    lv_obj_t *can1_row = lv_obj_create(can_card);
    lv_obj_set_size(can1_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(can1_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(can1_row, 0, 0);
    lv_obj_set_style_pad_all(can1_row, 0, 0);
    lv_obj_set_layout(can1_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(can1_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(can1_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *c1c = lv_label_create(can1_row);
    lv_label_set_text(c1c, "CAN1");
    lv_obj_add_style(c1c, &s_lbl_dim, 0);
    g_w.lbl_can1_status = lv_label_create(can1_row);
    lv_label_set_text(g_w.lbl_can1_status, LV_SYMBOL_OK " LIVE");
    lv_obj_set_style_text_color(g_w.lbl_can1_status, HMI_COL_GREEN, 0);
    lv_obj_set_style_text_font(g_w.lbl_can1_status, &lv_font_montserrat_10, 0);

    lv_obj_t *baud = lv_label_create(can_card);
    lv_label_set_text(baud, "J1939  |  250 kbps");
    lv_obj_add_style(baud, &s_lbl_hint, 0);

    /* Alarm card */
    lv_obj_t *alm = lv_obj_create(panel);
    lv_obj_set_size(alm, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(alm, &s_card_alarm, 0);
    lv_obj_set_layout(alm, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(alm, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(alm, 3, 0);

    lv_obj_t *alm_t = lv_label_create(alm);
    lv_label_set_text(alm_t, LV_SYMBOL_WARNING " ALARMS");
    lv_obj_set_style_text_color(alm_t, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(alm_t, &lv_font_montserrat_10, 0);

    g_w.alarm_ok = lv_label_create(alm);
    lv_label_set_text(g_w.alarm_ok, LV_SYMBOL_OK " All normal");
    lv_obj_set_style_text_color(g_w.alarm_ok, HMI_COL_GREEN, 0);
    lv_obj_set_style_text_font(g_w.alarm_ok, &lv_font_montserrat_10, 0);

    g_w.alarm_theft = lv_label_create(alm);
    lv_label_set_text(g_w.alarm_theft, "! FUEL THEFT");
    lv_obj_set_style_text_color(g_w.alarm_theft, HMI_COL_RED, 0);
    lv_obj_set_style_text_font(g_w.alarm_theft, &lv_font_montserrat_10, 0);
    lv_obj_add_flag(g_w.alarm_theft, LV_OBJ_FLAG_HIDDEN);

    g_w.alarm_temp = lv_label_create(alm);
    lv_label_set_text(g_w.alarm_temp, "! TEMP HIGH");
    lv_obj_set_style_text_color(g_w.alarm_temp, HMI_COL_RED, 0);
    lv_obj_set_style_text_font(g_w.alarm_temp, &lv_font_montserrat_10, 0);
    lv_obj_add_flag(g_w.alarm_temp, LV_OBJ_FLAG_HIDDEN);

    g_w.alarm_can = lv_label_create(alm);
    lv_label_set_text(g_w.alarm_can, "! CAN TIMEOUT");
    lv_obj_set_style_text_color(g_w.alarm_can, HMI_COL_RED, 0);
    lv_obj_set_style_text_font(g_w.alarm_can, &lv_font_montserrat_10, 0);
    lv_obj_add_flag(g_w.alarm_can, LV_OBJ_FLAG_HIDDEN);
}

/* ════════════════════════════════════════════════════════════
 *  Center panel — 5 metric cards (2+2+1) + chart
 * ════════════════════════════════════════════════════════════ */
static void build_center_panel(lv_obj_t *content)
{
    int center_w = HMI_SCREEN_W - HMI_LEFT_W - 10;

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_set_style_bg_color(panel, HMI_COL_BG, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_size(panel, center_w, HMI_CONTENT_H);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 6, 0);

    /* ── Section label ── */
    lv_obj_t *sec1 = lv_label_create(panel);
    lv_label_set_text(sec1, "FLOW METER DATA");
    lv_obj_set_style_text_color(sec1, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(sec1, &lv_font_montserrat_10, 0);

    /* ── Row 1: Supply | Return ── */
    lv_obj_t *row1 = lv_obj_create(panel);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_height(row1, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_layout(row1, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row1, 8, 0);

    /* Supply card */
    lv_obj_t *cs = lv_obj_create(row1);
    lv_obj_set_flex_grow(cs, 1);
    lv_obj_set_height(cs, LV_SIZE_CONTENT);
    lv_obj_add_style(cs, &s_card, 0);
    lv_obj_set_layout(cs, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cs, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cs, 2, 0);
    lv_obj_t *cs_cap = lv_label_create(cs);
    lv_label_set_text(cs_cap, "FLOW SUPPLY");
    lv_obj_add_style(cs_cap, &s_lbl_hint, 0);
    g_w.lbl_flow_supply = lv_label_create(cs);
    lv_label_set_text(g_w.lbl_flow_supply, "148 L/h");
    lv_obj_add_style(g_w.lbl_flow_supply, &s_lbl_blue, 0);

    /* Return card */
    lv_obj_t *cr = lv_obj_create(row1);
    lv_obj_set_flex_grow(cr, 1);
    lv_obj_set_height(cr, LV_SIZE_CONTENT);
    lv_obj_add_style(cr, &s_card, 0);
    lv_obj_set_style_border_color(cr, lv_color_make(80,60,0), 0);
    lv_obj_set_layout(cr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cr, 2, 0);
    lv_obj_t *cr_cap = lv_label_create(cr);
    lv_label_set_text(cr_cap, "FLOW RETURN");
    lv_obj_add_style(cr_cap, &s_lbl_hint, 0);
    g_w.lbl_flow_return = lv_label_create(cr);
    lv_label_set_text(g_w.lbl_flow_return, "112 L/h");
    lv_obj_add_style(g_w.lbl_flow_return, &s_lbl_accent, 0);

    /* ── Row 2: Net | Total | Range ── */
    lv_obj_t *row2 = lv_obj_create(panel);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_height(row2, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_layout(row2, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row2, 8, 0);

    /* Net */
    lv_obj_t *cn = lv_obj_create(row2);
    lv_obj_set_flex_grow(cn, 1);
    lv_obj_set_height(cn, LV_SIZE_CONTENT);
    lv_obj_add_style(cn, &s_card, 0);
    lv_obj_set_layout(cn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cn, 2, 0);
    lv_obj_t *cn_cap = lv_label_create(cn);
    lv_label_set_text(cn_cap, "NET CONSUMPTION");
    lv_obj_add_style(cn_cap, &s_lbl_hint, 0);
    g_w.lbl_net_flow = lv_label_create(cn);
    lv_label_set_text(g_w.lbl_net_flow, "36 L/h");
    lv_obj_add_style(g_w.lbl_net_flow, &s_lbl_green, 0);
    lv_obj_t *cn_sub = lv_label_create(cn);
    lv_label_set_text(cn_sub, "SUPPLY - RETURN");
    lv_obj_add_style(cn_sub, &s_lbl_hint, 0);

    /* Total */
    lv_obj_t *ct = lv_obj_create(row2);
    lv_obj_set_flex_grow(ct, 1);
    lv_obj_set_height(ct, LV_SIZE_CONTENT);
    lv_obj_add_style(ct, &s_card, 0);
    lv_obj_set_style_border_color(ct, lv_color_make(60,20,20), 0);
    lv_obj_set_layout(ct, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ct, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ct, 2, 0);
    lv_obj_t *ct_cap = lv_label_create(ct);
    lv_label_set_text(ct_cap, "TOTAL USED");
    lv_obj_add_style(ct_cap, &s_lbl_hint, 0);
    g_w.lbl_total_used = lv_label_create(ct);
    lv_label_set_text(g_w.lbl_total_used, "284 L");
    lv_obj_add_style(g_w.lbl_total_used, &s_lbl_red, 0);
    lv_obj_t *ct_sub = lv_label_create(ct);
    lv_label_set_text(ct_sub, "SESSION ACCUM.");
    lv_obj_add_style(ct_sub, &s_lbl_hint, 0);

    /* Range */
    lv_obj_t *crng = lv_obj_create(row2);
    lv_obj_set_flex_grow(crng, 1);
    lv_obj_set_height(crng, LV_SIZE_CONTENT);
    lv_obj_add_style(crng, &s_card, 0);
    lv_obj_set_style_border_color(crng, lv_color_make(80,50,0), 0);
    lv_obj_set_layout(crng, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(crng, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(crng, 2, 0);
    lv_obj_t *crng_cap = lv_label_create(crng);
    lv_label_set_text(crng_cap, "EST. RANGE");
    lv_obj_add_style(crng_cap, &s_lbl_hint, 0);
    g_w.lbl_range = lv_label_create(crng);
    lv_label_set_text(g_w.lbl_range, "79 HRS");
    lv_obj_add_style(g_w.lbl_range, &s_lbl_accent, 0);

    /* ── Section: Chart ── */
    lv_obj_t *sec2 = lv_label_create(panel);
    lv_label_set_text(sec2, "CONSUMPTION TREND");
    lv_obj_set_style_text_color(sec2, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(sec2, &lv_font_montserrat_10, 0);

    g_w.chart = lv_chart_create(panel);
    lv_obj_set_size(g_w.chart, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(g_w.chart, 1);
    lv_chart_set_type(g_w.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_w.chart, 60);
    lv_chart_set_range(g_w.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 250);
    lv_chart_set_div_line_count(g_w.chart, 4, 0);
    lv_obj_set_style_bg_color(g_w.chart, lv_color_make(17,17,17), 0);
    lv_obj_set_style_bg_opa(g_w.chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_w.chart, HMI_COL_BORDER, 0);
    lv_obj_set_style_border_width(g_w.chart, 1, 0);
    lv_obj_set_style_line_color(g_w.chart, lv_color_make(30,30,30), LV_PART_MAIN);
    lv_obj_set_style_line_width(g_w.chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(g_w.chart, 0, LV_PART_INDICATOR);

    g_w.ser_supply = lv_chart_add_series(g_w.chart, HMI_COL_BLUE,
                                          LV_CHART_AXIS_PRIMARY_Y);
    g_w.ser_return = lv_chart_add_series(g_w.chart, HMI_COL_ACCENT,
                                          LV_CHART_AXIS_PRIMARY_Y);
    g_w.ser_net    = lv_chart_add_series(g_w.chart, HMI_COL_GREEN,
                                          LV_CHART_AXIS_PRIMARY_Y);

    /* Legend */
    lv_obj_t *legend = lv_obj_create(panel);
    lv_obj_set_size(legend, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_layout(legend, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend, 16, 0);
    const char   *ln[] = {"- SUPPLY", "- RETURN", "- NET"};
    lv_color_t    lc[] = {HMI_COL_BLUE, HMI_COL_ACCENT, HMI_COL_GREEN};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *li = lv_label_create(legend);
        lv_label_set_text(li, ln[i]);
        lv_obj_set_style_text_color(li, lc[i], 0);
        lv_obj_set_style_text_font(li, &lv_font_montserrat_10, 0);
    }
}

/* ════════════════════════════════════════════════════════════
 *  Bottom bar
 * ════════════════════════════════════════════════════════════ */
static void build_botbar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_add_style(bar, &s_botbar, 0);
    lv_obj_set_size(bar, HMI_SCREEN_W, HMI_BOTBAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l = lv_label_create(bar);
    lv_label_set_text(l, "ESP32-S3 | LVGL v8 | RGB 800x480");
    lv_obj_add_style(l, &s_lbl_hint, 0);

    g_w.lbl_uptime = lv_label_create(bar);
    lv_label_set_text(g_w.lbl_uptime, "UPTIME: 00:00:00");
    lv_obj_set_style_text_color(g_w.lbl_uptime, HMI_COL_ACCENT, 0);
    lv_obj_set_style_text_font(g_w.lbl_uptime, &lv_font_montserrat_10, 0);

    lv_obj_t *r = lv_label_create(bar);
    lv_label_set_text(r, "WAVESHARE 7\" | FMC650 TELTONIKA");
    lv_obj_add_style(r, &s_lbl_hint, 0);
}

/* ════════════════════════════════════════════════════════════
 *  PUBLIC: fuel_hmi_create
 * ════════════════════════════════════════════════════════════ */
void fuel_hmi_create(void)
{
    styles_init();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &s_screen, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    build_topbar(scr);
    build_botbar(scr);

    /* Content between bars */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, HMI_SCREEN_W, HMI_CONTENT_H);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, HMI_TOPBAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    build_left_panel(content);
    build_center_panel(content);
}

/* ════════════════════════════════════════════════════════════
 *  PUBLIC: fuel_hmi_update
 * ════════════════════════════════════════════════════════════ */
void fuel_hmi_update(const hmi_fuel_data_t *d)
{
    char buf[32];

    /* Arc + pct */
    lv_arc_set_value(g_w.arc_fuel, (int)d->fuel_level_pct);
    snprintf(buf, sizeof(buf), "%.0f%%", d->fuel_level_pct);
    lv_label_set_text(g_w.lbl_fuel_pct, buf);

    /* Volume bar */
    lv_bar_set_value(g_w.bar_fuel_vol, (int)d->fuel_volume_liters, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%.0f L", d->fuel_volume_liters);
    lv_label_set_text(g_w.lbl_fuel_vol, buf);

    /* Temp */
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", d->fuel_temp_celsius);
    lv_label_set_text(g_w.lbl_fuel_temp, buf);
    lv_color_t tcol = (d->fuel_temp_celsius > 55.0f) ? HMI_COL_RED : HMI_COL_TEXT_PRI;
    lv_obj_set_style_text_color(g_w.lbl_fuel_temp, tcol, 0);

    /* CAN status */
    if (d->can1_alive) {
        lv_label_set_text(g_w.dot_can1,       LV_SYMBOL_OK " CAN BUS OK");
        lv_obj_set_style_text_color(g_w.dot_can1, HMI_COL_GREEN, 0);
        lv_label_set_text(g_w.lbl_can1_status, LV_SYMBOL_OK " LIVE");
        lv_obj_set_style_text_color(g_w.lbl_can1_status, HMI_COL_GREEN, 0);
    } else {
        lv_label_set_text(g_w.dot_can1,       LV_SYMBOL_CLOSE " CAN FAULT");
        lv_obj_set_style_text_color(g_w.dot_can1, HMI_COL_RED, 0);
        lv_label_set_text(g_w.lbl_can1_status, LV_SYMBOL_CLOSE " FAULT");
        lv_obj_set_style_text_color(g_w.lbl_can1_status, HMI_COL_RED, 0);
    }

    /* Flow cards */
    snprintf(buf, sizeof(buf), "%.0f L/h", d->flow_supply_lph);
    lv_label_set_text(g_w.lbl_flow_supply, buf);

    snprintf(buf, sizeof(buf), "%.0f L/h", d->flow_return_lph);
    lv_label_set_text(g_w.lbl_flow_return, buf);

    snprintf(buf, sizeof(buf), "%.0f L/h", d->flow_net_lph);
    lv_label_set_text(g_w.lbl_net_flow, buf);

    snprintf(buf, sizeof(buf), "%.0f L", d->total_consumed_L);
    lv_label_set_text(g_w.lbl_total_used, buf);

    snprintf(buf, sizeof(buf), "%lu HRS", (unsigned long)d->est_range_hours);
    lv_label_set_text(g_w.lbl_range, buf);

    /* Chart */
    lv_chart_set_next_value(g_w.chart, g_w.ser_supply, (lv_coord_t)d->flow_supply_lph);
    lv_chart_set_next_value(g_w.chart, g_w.ser_return, (lv_coord_t)d->flow_return_lph);
    lv_chart_set_next_value(g_w.chart, g_w.ser_net,    (lv_coord_t)d->flow_net_lph);
    lv_chart_refresh(g_w.chart);

    /* Alarms */
    bool theft   = (d->sensor_status & HMI_ALARM_THEFT)     != 0;
    bool temph   = (d->sensor_status & HMI_ALARM_TEMP_HIGH) != 0;
    bool can_err = !d->can1_alive || (d->sensor_status & HMI_ALARM_CAN_TIMEOUT) != 0;
    bool any     = theft || temph || can_err;

    if (theft)   lv_obj_clear_flag(g_w.alarm_theft, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (g_w.alarm_theft, LV_OBJ_FLAG_HIDDEN);
    if (temph)   lv_obj_clear_flag(g_w.alarm_temp,  LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (g_w.alarm_temp,  LV_OBJ_FLAG_HIDDEN);
    if (can_err) lv_obj_clear_flag(g_w.alarm_can,   LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (g_w.alarm_can,   LV_OBJ_FLAG_HIDDEN);
    if (!any)    lv_obj_clear_flag(g_w.alarm_ok,    LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (g_w.alarm_ok,    LV_OBJ_FLAG_HIDDEN);
}

/* ════════════════════════════════════════════════════════════
 *  PUBLIC: fuel_hmi_tick_uptime
 * ════════════════════════════════════════════════════════════ */
void fuel_hmi_tick_uptime(uint32_t uptime_sec)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "UPTIME: %02lu:%02lu:%02lu",
             (unsigned long)(uptime_sec / 3600),
             (unsigned long)((uptime_sec % 3600) / 60),
             (unsigned long)(uptime_sec % 60));
    lv_label_set_text(g_w.lbl_uptime, buf);
}