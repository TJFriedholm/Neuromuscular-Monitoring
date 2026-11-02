#include "widgets.hpp"

#include <cstring>

namespace ui::w {

lv_color_t tone_color(Tone t) {
    switch (t) {
        case Tone::Normal: return color::kText;
        case Tone::Muted: return color::kMuted;
        case Tone::Faint: return color::kGrey;
        case Tone::Accent: return color::kCyan;
        case Tone::Good: return color::kGreen;
        case Tone::Caution: return color::kAmber;
        case Tone::Alert: return color::kRed;
    }
    return color::kText;
}

lv_color_t tone_border(Tone t) {
    switch (t) {
        case Tone::Good: return lv_color_hex(0x7CC49C);
        case Tone::Caution: return lv_color_hex(0xE0B870);
        case Tone::Alert: return lv_color_hex(0xD9716A);
        default: return color::kLine;
    }
}

lv_color_t tone_chip(Tone t) {
    switch (t) {
        case Tone::Good: return lv_color_hex(0xE3F3EA);
        case Tone::Caution: return lv_color_hex(0xFBEFD9);
        case Tone::Alert: return lv_color_hex(0xFBE5E3);
        default: return lv_color_hex(0xEDF0F3);
    }
}

lv_obj_t* box(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    // Not a touch target, so a press inside a button reaches the button.
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t* row(lv_obj_t* parent, int32_t gap, lv_flex_align_t cross) {
    lv_obj_t* o = box(parent);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, cross, cross);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}

lv_obj_t* column(lv_obj_t* parent, int32_t gap) {
    lv_obj_t* o = box(parent);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}

void spacer(lv_obj_t* parent) {
    lv_obj_t* o = box(parent);
    lv_obj_set_size(o, 0, 0);
    lv_obj_set_flex_grow(o, 1);
}

lv_obj_t* page(lv_obj_t* body, int32_t pad_ver) {
    lv_obj_t* p = box(body);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(p, kSide, 0);
    lv_obj_set_style_pad_ver(p, pad_ver, 0);
    return p;
}

void card(lv_obj_t* o, int32_t border) {
    lv_obj_set_style_bg_color(o, color::kPanel, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, color::kLine, 0);
    lv_obj_set_style_border_width(o, border, 0);
    lv_obj_set_style_radius(o, kRadius, 0);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t c, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, text);
    return l;
}

void set_text(lv_obj_t* l, const char* text) {
    if (std::strcmp(lv_label_get_text(l), text) != 0) {
        lv_label_set_text(l, text);
    }
}

void set_visible(lv_obj_t* o, bool visible) {
    if (visible) {
        lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

void set_enabled(lv_obj_t* o, bool enabled) {
    lv_obj_set_state(o, LV_STATE_DISABLED, !enabled);
}

// ---- Icons ------------------------------------------------------------------

namespace {

// Icons are polylines on a 24-unit grid (the mockup's SVG viewBox), stored in
// tenths so they scale to any size without float support in LVGL.
struct Poly {
    int n;
    lv_point_t pts[6];
};
struct IconDef {
    int polys;
    Poly poly[3];
    int stroke_tenths;
};

const IconDef& def(Icon i) {
    static const IconDef kRight = {1, {{3, {{90, 50}, {160, 120}, {90, 190}}}}, 24};
    static const IconDef kLeft = {1, {{3, {{150, 50}, {80, 120}, {150, 190}}}}, 24};
    static const IconDef kMenu = {3,
                                  {{2, {{40, 70}, {200, 70}}},
                                   {2, {{40, 120}, {200, 120}}},
                                   {2, {{40, 170}, {200, 170}}}},
                                  22};
    static const IconDef kCheck = {1, {{3, {{40, 125}, {95, 180}, {200, 65}}}}, 26};
    static const IconDef kDelete = {
        3,
        {{6, {{90, 50}, {210, 50}, {210, 190}, {90, 190}, {30, 120}, {90, 50}}},
         {2, {{120, 90}, {180, 150}}},
         {2, {{180, 90}, {120, 150}}}},
        20};
    static const IconDef kWarning = {
        3,
        {{4, {{120, 23}, {223, 206}, {17, 206}, {120, 23}}},
         {2, {{120, 91}, {120, 143}}},
         {2, {{120, 171}, {120, 172}}}},
        16};
    switch (i) {
        case Icon::ChevronRight: return kRight;
        case Icon::ChevronLeft: return kLeft;
        case Icon::Menu: return kMenu;
        case Icon::Check: return kCheck;
        case Icon::Delete: return kDelete;
        case Icon::Warning: return kWarning;
    }
    return kRight;
}

}  // namespace

lv_obj_t* icon(lv_obj_t* parent, Icon i, lv_color_t c, int32_t size) {
    lv_obj_t* holder = box(parent);
    lv_obj_set_size(holder, size, size);

    const IconDef& d = def(i);
    const int32_t stroke = (d.stroke_tenths * size + 120) / 240;
    for (int p = 0; p < d.polys; ++p) {
        const Poly& poly = d.poly[p];
        // lv_line keeps a pointer to its points, so they must outlive it.
        auto* pts = new lv_point_precise_t[poly.n];
        for (int k = 0; k < poly.n; ++k) {
            pts[k].x = (poly.pts[k].x * size + 120) / 240;
            pts[k].y = (poly.pts[k].y * size + 120) / 240;
        }
        lv_obj_t* l = lv_line_create(holder);
        lv_line_set_points(l, pts, static_cast<uint32_t>(poly.n));
        lv_obj_set_pos(l, 0, 0);
        lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_line_color(l, c, 0);
        lv_obj_set_style_line_width(l, stroke < 1 ? 1 : stroke, 0);
        lv_obj_set_style_line_rounded(l, true, 0);
        lv_obj_add_event_cb(
            l, [](lv_event_t* e) { delete[] static_cast<lv_point_precise_t*>(lv_event_get_user_data(e)); },
            LV_EVENT_DELETE, pts);
    }
    return holder;
}

// ---- Buttons and events -----------------------------------------------------

lv_obj_t* button(lv_obj_t* parent, int32_t width) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, width, kTouchMin);
    lv_obj_set_style_radius(b, kRadius, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, color::kWhite, 0);
    lv_obj_set_style_border_color(b, color::kLine2, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_text_font(b, &plex_sans_32_semibold, 0);
    lv_obj_set_style_text_color(b, color::kSoft, 0);
    lv_obj_set_style_text_letter_space(b, 1, 0);
    // Pressed feedback: a darker fill while the finger is down.
    lv_obj_set_style_bg_color(b, color::kLine, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, color::kOffFill, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(b, color::kOffLine, LV_STATE_DISABLED);
    lv_obj_set_style_border_width(b, 1, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(b, color::kOffText, LV_STATE_DISABLED);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 14, 0);
    return b;
}

void on_event(lv_obj_t* obj, lv_event_code_t code, std::function<void()> fn) {
    // Owned by the object: freed when LVGL deletes it.
    auto* held = new std::function<void()>(std::move(fn));
    lv_obj_add_event_cb(
        obj, [](lv_event_t* e) { (*static_cast<std::function<void()>*>(lv_event_get_user_data(e)))(); },
        code, held);
    lv_obj_add_event_cb(
        obj, [](lv_event_t* e) { delete static_cast<std::function<void()>*>(lv_event_get_user_data(e)); },
        LV_EVENT_DELETE, held);
}

void on_click(lv_obj_t* obj, std::function<void()> fn) {
    on_event(obj, LV_EVENT_CLICKED, std::move(fn));
}

}  // namespace ui::w
