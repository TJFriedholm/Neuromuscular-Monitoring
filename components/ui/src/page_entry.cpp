// Typed entry: a numeric keypad for one setting.

#include <cstdio>

#include "correction/correction.hpp"
#include "pages.hpp"
#include "widgets.hpp"

namespace ui {

using namespace w;

namespace {

const lv_color_t kHelpColor = lv_color_hex(0x8A5A00);

lv_obj_t* key(lv_obj_t* parent) {
    lv_obj_t* k = button(parent, kTouchMin);
    lv_obj_set_style_text_font(k, &plex_mono_46_medium, 0);
    lv_obj_set_style_text_color(k, color::kText, 0);
    lv_obj_set_style_text_letter_space(k, 0, 0);
    return k;
}

}  // namespace

void EntryPage::create(lv_obj_t* body, App& app) {
    root = page(body, 10);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(root, kTouchGap, 0);

    // Left: what has been typed, its bounds, and the current value.
    lv_obj_t* panel = column(root, 12);
    card(panel);
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_style_pad_hor(panel, 24, 0);
    lv_obj_set_style_pad_ver(panel, 20, 0);
    label(panel, &plex_sans_32, color::kMuted, "New value");
    field_box_ = row(panel, 14);
    lv_obj_set_size(field_box_, lv_pct(100), 136);
    lv_obj_set_style_pad_hor(field_box_, 22, 0);
    lv_obj_set_style_radius(field_box_, 10, 0);
    lv_obj_set_style_bg_color(field_box_, color::kWhite, 0);
    lv_obj_set_style_bg_opa(field_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(field_box_, 3, 0);
    typed_ = label(field_box_, &plex_mono_120_semibold, color::kText);
    unit_ = label(field_box_, &plex_sans_46_medium, color::kMuted);
    message_ = label(panel, &plex_sans_32, color::kSub);
    current_ = label(panel, &plex_sans_32, color::kSub);
    spacer(panel);
    help_ = label(panel, &plex_sans_32, kHelpColor);
    lv_obj_set_width(help_, lv_pct(100));
    lv_label_set_long_mode(help_, LV_LABEL_LONG_MODE_WRAP);

    // Right: keypad, four keys a row, each 116 px and 58 px apart (§5.2.3).
    lv_obj_t* pad = box(root);
    lv_obj_set_size(pad, 4 * kTouchMin + 3 * kTouchGap, lv_pct(100));
    lv_obj_set_flex_flow(pad, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(pad, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pad, kTouchGap, 0);
    lv_obj_set_style_pad_row(pad, kTouchGap, 0);

    // Same order as the mockup: 789⌫ / 456. / 1230
    const char kLayout[] = "789<456.1230";
    for (const char c : kLayout) {
        if (c == '\0') {
            break;
        }
        lv_obj_t* k = key(pad);
        if (c == '<') {
            icon(k, Icon::Delete, color::kSoft, 44);
            on_click(k, [&app] { press_delete(app.entry()); });
        } else if (c == '.') {
            lv_label_set_text(lv_label_create(k), ".");
            on_click(k, [&app] { press_point(app.entry()); });
            point_key_ = k;
        } else {
            char text[2] = {c, '\0'};
            lv_label_set_text(lv_label_create(k), text);
            on_click(k, [c, &app] { press_digit(app.entry(), c); });
        }
    }
}

void EntryPage::show(const Snapshot& s, const Entry& e) {
    const FieldSpec& fs = spec(e.field);
    const EntryCheck c = check(e);
    const float current = value_of(s.settings, e.field);
    const bool typed = e.buf[0] != '\0';

    char text[16];
    if (typed) {
        std::snprintf(text, sizeof text, "%s", e.buf);
    } else {
        std::snprintf(text, sizeof text, "%.*f", fs.decimals, static_cast<double>(current));
    }
    set_text(typed_, text);
    lv_obj_set_style_text_color(typed_, typed ? color::kText : color::kGrey, 0);
    set_text(unit_, fs.unit);
    lv_obj_set_style_border_color(field_box_,
                                  !typed ? color::kLine2 : (c.valid ? color::kCyan : color::kRed), 0);

    set_text(message_, c.message);
    lv_obj_set_style_text_color(message_, c.error ? color::kRed : color::kSub, 0);
    char cur[40];
    std::snprintf(cur, sizeof cur, "Current %.*f%s%s", fs.decimals, static_cast<double>(current),
                  fs.unit[0] != '\0' ? " " : "", fs.unit);
    set_text(current_, cur);

    // Warnings for the settings whose values are not yet sourced.
    char help[96] = "";
    if (e.field == Field::Exponent) {
        const float n = c.valid ? c.value : current;
        const float lim = s.settings.alarm_limit;
        std::snprintf(help, sizeof help, "Needs facial %.3f to show %.2f. No validated default yet.",
                      static_cast<double>(correction::facial_needed_for(lim, n)),
                      static_cast<double>(lim));
    } else if (e.field == Field::Theta) {
        std::snprintf(help, sizeof help, "Provisional until bench values exist.");
    }
    set_text(help_, help);
    set_visible(help_, help[0] != '\0');

    set_enabled(point_key_, fs.decimals > 0);
}

}  // namespace ui
