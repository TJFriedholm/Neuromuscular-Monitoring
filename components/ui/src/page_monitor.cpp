// Monitor (the home page) and Signal.

#include <algorithm>

#include "pages.hpp"
#include "widgets.hpp"

namespace ui {

using namespace w;

lv_obj_t* make_trace(lv_obj_t* parent, lv_chart_series_t** series) {
    lv_obj_t* chart = lv_chart_create(parent);
    lv_obj_remove_style_all(chart);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE);  // let presses reach the parent
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, kTracePoints);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, -kTraceRangeUv, kTraceRangeUv);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);  // no point markers
    *series = lv_chart_add_series(chart, color::kCyan, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(chart, *series, 0);
    return chart;
}

// ---- Monitor ------------------------------------------------------------------

void MonitorPage::create(lv_obj_t* body, App& app) {
    root = page(body, 20);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(root, 20, 0);

    lv_obj_t* top = row(root, 20, LV_FLEX_ALIGN_START);
    lv_obj_set_size(top, lv_pct(100), 300);

    // Ratio card: Peripheral-Equivalent TOF Ratio, headed "Train of Four ratio",
    // with the Facial TOF Ratio it was derived from underneath (ADR-0009).
    ratio_card_ = column(top, 10);
    card(ratio_card_, 2);
    lv_obj_set_size(ratio_card_, 580, lv_pct(100));
    lv_obj_set_style_pad_hor(ratio_card_, 24, 0);
    lv_obj_set_style_pad_ver(ratio_card_, 18, 0);
    label(ratio_card_, &plex_sans_32, color::kMuted, "Train of Four ratio");
    lv_obj_t* value_row = row(ratio_card_, 24);
    lv_obj_set_size(value_row, lv_pct(100), LV_SIZE_CONTENT);
    peripheral_ = label(value_row, &plex_mono_120_semibold, color::kGrey);
    chip_ = label(value_row, &plex_sans_32_semibold, color::kGrey);
    lv_obj_set_style_pad_hor(chip_, 14, 0);
    lv_obj_set_style_pad_ver(chip_, 6, 0);
    lv_obj_set_style_radius(chip_, 8, 0);
    lv_obj_set_style_bg_opa(chip_, LV_OPA_COVER, 0);
    spacer(ratio_card_);
    facial_row_ = row(ratio_card_, 16, LV_FLEX_ALIGN_END);
    lv_obj_set_size(facial_row_, lv_pct(100), LV_SIZE_CONTENT);
    label(facial_row_, &plex_sans_32, color::kMuted, "Facial");
    facial_ = label(facial_row_, &plex_mono_46_medium, color::kSoft);
    ratio_note_ = label(ratio_card_, &plex_sans_32, color::kMuted);

    // TOF Count card. Wide enough for "Twitch count" at 32 px (186 px).
    lv_obj_t* count_card = column(top, 0);
    card(count_card);
    lv_obj_set_size(count_card, 230, lv_pct(100));
    lv_obj_set_style_pad_hor(count_card, 20, 0);
    lv_obj_set_style_pad_ver(count_card, 18, 0);
    lv_obj_set_flex_align(count_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    label(count_card, &plex_sans_32, color::kMuted, "Twitch count");
    spacer(count_card);
    count_ = label(count_card, &plex_mono_140_semibold, color::kGrey);
    spacer(count_card);

    // Twitch card: T1-T4 bars.
    lv_obj_t* twitch_card = column(top, 8);
    card(twitch_card);
    lv_obj_set_height(twitch_card, lv_pct(100));
    lv_obj_set_flex_grow(twitch_card, 1);
    lv_obj_set_style_pad_hor(twitch_card, 24, 0);
    lv_obj_set_style_pad_ver(twitch_card, 18, 0);
    label(twitch_card, &plex_sans_32, color::kMuted, "Twitches");
    lv_obj_t* tracks = row(twitch_card, 16, LV_FLEX_ALIGN_END);
    lv_obj_set_size(tracks, lv_pct(100), 112);
    lv_obj_t* names = row(twitch_card, 16);
    lv_obj_set_size(names, lv_pct(100), LV_SIZE_CONTENT);
    for (int i = 0; i < tof::kTwitchCount; ++i) {
        lv_obj_t* track = box(tracks);
        lv_obj_set_height(track, lv_pct(100));
        lv_obj_set_flex_grow(track, 1);
        lv_obj_set_style_radius(track, 6, 0);
        lv_obj_set_style_bg_color(track, color::kTrack, 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        fills_[i] = box(track);
        lv_obj_set_width(fills_[i], lv_pct(100));
        lv_obj_align(fills_[i], LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(fills_[i], 6, 0);
        lv_obj_set_style_bg_opa(fills_[i], LV_OPA_COVER, 0);

        names_[i] = label(names, &plex_sans_32_semibold, color::kDim);
        lv_obj_set_flex_grow(names_[i], 1);
        lv_obj_set_style_text_align(names_[i], LV_TEXT_ALIGN_CENTER, 0);
    }

    // EMG strip. The whole strip is the button into the Signal page.
    lv_obj_t* strip = button(root, 0);
    lv_obj_set_size(strip, lv_pct(100), 124);
    lv_obj_set_style_bg_color(strip, color::kPanel, 0);
    lv_obj_set_style_text_color(strip, color::kMuted, 0);
    lv_obj_set_style_text_font(strip, &plex_sans_32, 0);
    lv_obj_set_style_text_letter_space(strip, 0, 0);
    lv_obj_set_style_pad_hor(strip, 24, 0);
    lv_obj_set_style_pad_column(strip, 24, 0);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(strip), "EMG");
    trace_ = make_trace(strip, &series_);
    lv_obj_set_height(trace_, 92);
    lv_obj_set_flex_grow(trace_, 1);
    icon(strip, Icon::ChevronRight, color::kMuted);
    on_click(strip, [&app] { app.go(Page::Signal); });
}

void MonitorPage::show(const MonitorView& v) {
    set_text(peripheral_, v.peripheral);
    lv_obj_set_style_text_color(peripheral_, tone_color(v.ratio_tone), 0);
    lv_obj_set_style_border_color(ratio_card_, tone_border(v.ratio_tone), 0);
    set_visible(chip_, v.show_chip);
    set_text(chip_, v.chip);
    lv_obj_set_style_text_color(chip_, tone_color(v.ratio_tone), 0);
    lv_obj_set_style_bg_color(chip_, tone_chip(v.ratio_tone), 0);

    set_visible(facial_row_, v.show_facial);
    set_visible(ratio_note_, !v.show_facial);
    set_text(facial_, v.facial);
    set_text(ratio_note_, v.ratio_note);

    set_text(count_, v.count);
    lv_obj_set_style_text_color(count_, tone_color(v.count_tone), 0);

    for (int i = 0; i < tof::kTwitchCount; ++i) {
        const auto& b = v.bars[i];
        lv_obj_set_height(fills_[i], lv_pct(static_cast<int32_t>(b.fill * 100.0f + 0.5f)));
        lv_obj_set_style_bg_color(fills_[i], b.detected ? color::kCyan : color::kLine2, 0);
        set_text(names_[i], b.name);
        lv_obj_set_style_text_color(names_[i], b.detected ? color::kText : color::kDim, 0);
    }
}

void MonitorPage::set_trace(const std::int32_t* uv, std::size_t n) {
    lv_chart_set_series_values(trace_, series_, uv, std::min(n, kTracePoints));
}

// ---- Signal ---------------------------------------------------------------------

void SignalPage::create(lv_obj_t* body) {
    root = page(body, 20);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(root, 20, 0);

    lv_obj_t* plot = column(root, 8);
    card(plot);
    lv_obj_set_size(plot, lv_pct(100), 280);
    lv_obj_set_style_pad_hor(plot, 24, 0);
    lv_obj_set_style_pad_ver(plot, 18, 0);
    label(plot, &plex_sans_32, color::kMuted, "EMG, orbicularis oculi");
    trace_ = make_trace(plot, &series_);
    lv_obj_set_width(trace_, lv_pct(100));
    lv_obj_set_flex_grow(trace_, 1);
    lv_chart_set_div_line_count(trace_, 1, 0);  // zero line
    lv_obj_set_style_line_color(trace_, color::kLine, LV_PART_MAIN);
    lv_obj_set_style_line_width(trace_, 1, LV_PART_MAIN);

    lv_obj_t* cards = row(root, 20, LV_FLEX_ALIGN_START);
    lv_obj_set_size(cards, lv_pct(100), 144);
    for (int i = 0; i < tof::kTwitchCount; ++i) {
        lv_obj_t* c = column(cards, 0);
        card(c);
        lv_obj_set_height(c, lv_pct(100));
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_style_pad_hor(c, 20, 0);
        lv_obj_set_style_pad_ver(c, 16, 0);
        names_[i] = label(c, &plex_sans_32_semibold, color::kDim);
        spacer(c);
        lv_obj_t* r = row(c, 10, LV_FLEX_ALIGN_END);
        lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
        values_[i] = label(r, &plex_mono_46_medium, color::kGrey);
        label(r, &plex_sans_32, color::kMuted, "mV");
    }
}

void SignalPage::show(const MonitorView& v) {
    for (int i = 0; i < tof::kTwitchCount; ++i) {
        const auto& b = v.bars[i];
        set_text(names_[i], b.name);
        lv_obj_set_style_text_color(names_[i], b.detected ? color::kText : color::kDim, 0);
        set_text(values_[i], b.value);
        lv_obj_set_style_text_color(values_[i], b.detected ? color::kSoft : color::kGrey, 0);
    }
}

void SignalPage::set_trace(const std::int32_t* uv, std::size_t n) {
    lv_chart_set_series_values(trace_, series_, uv, std::min(n, kTracePoints));
}

}  // namespace ui
