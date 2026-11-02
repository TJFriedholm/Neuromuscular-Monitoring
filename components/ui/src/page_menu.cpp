// Menu, the settings pages, and the Fault page.

#include <cstdio>

#include "pages.hpp"
#include "widgets.hpp"

namespace ui {

using namespace w;

namespace {

// Menu tiles sit three to a row, settings tiles two to a row, 58 px apart.
constexpr int32_t kContentW = kScreenW - 2 * kSide;
constexpr int32_t kMenuTileW = (kContentW - 2 * kTouchGap) / 3;
constexpr int32_t kSettingTileW = (kContentW - kTouchGap) / 2;

lv_obj_t* wrapping_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t c,
                         const char* text = "") {
    lv_obj_t* l = label(parent, font, c, text);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return l;
}

}  // namespace

// ---- Menu and settings tiles --------------------------------------------------

void TilesPage::create(lv_obj_t* body, App& app) {
    root = page(body);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(root, kTouchGap, 0);
    lv_obj_set_style_pad_row(root, kTouchGap, 0);

    for (int i = 0; i < kMaxTiles; ++i) {
        TileWidgets& t = tiles_[i];
        t.button = button(root, kMenuTileW);
        lv_obj_set_height(t.button, kTileH);
        lv_obj_set_flex_align(t.button, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(t.button, 28, 0);
        lv_obj_set_style_pad_column(t.button, 16, 0);
        lv_obj_set_style_text_letter_space(t.button, 0, 0);

        lv_obj_t* text = column(t.button, 14);
        lv_obj_set_height(text, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(text, 1);
        t.name = label(text, &plex_sans_32, color::kSub);
        t.value = label(text, &plex_mono_46_medium, color::kText);
        t.chevron = icon(t.button, Icon::ChevronRight, color::kMuted);

        on_click(t.button, [this, i, &app] { app.activate(targets_[i]); });
    }
}

void TilesPage::show(Page p, const Snapshot& s) {
    Tile tiles[kMaxTiles];
    const int n = tiles_for(p, s, tiles);
    const bool menu = p == Page::Menu;

    // Menu tiles are a name only, larger; settings tiles show name and value.
    if (laid_out_for_ != p) {
        laid_out_for_ = p;
        for (TileWidgets& t : tiles_) {
            lv_obj_set_width(t.button, menu ? kMenuTileW : kSettingTileW);
            lv_obj_set_style_text_font(t.name, menu ? &plex_sans_32_semibold : &plex_sans_32, 0);
            lv_obj_set_style_text_color(t.name, menu ? color::kText : color::kSub, 0);
            set_visible(t.value, !menu);
        }
    }

    for (int i = 0; i < kMaxTiles; ++i) {
        TileWidgets& t = tiles_[i];
        set_visible(t.button, i < n);
        if (i >= n) {
            continue;
        }
        const Tile& tile = tiles[i];
        targets_[i] = tile.target;
        set_text(t.name, tile.name);
        set_text(t.value, tile.value);
        lv_obj_set_style_text_color(t.value, tone_color(tile.value_tone), 0);
        set_visible(t.chevron, tile.editable);
        // A read-only tile shows a value but is not a button: no fill, a faint
        // outline, and it does not respond to touch.
        if (tile.editable) {
            lv_obj_add_flag(t.button, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(t.button, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(t.button, color::kPanel, 0);
            lv_obj_set_style_border_color(t.button, color::kLine2, 0);
        } else {
            lv_obj_remove_flag(t.button, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(t.button, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(t.button, color::kOffLine, 0);
        }
    }
}

// ---- Electrodes ---------------------------------------------------------------

void ElectrodesPage::create(lv_obj_t* body) {
    root = page(body);
    card_ = column(root, 16);
    card(card_, 2);
    lv_obj_set_size(card_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(card_, 32, 0);
    lv_obj_set_style_pad_ver(card_, 28, 0);
    label(card_, &plex_sans_32, color::kMuted, "Skin-electrode impedance");
    lv_obj_t* r = row(card_, 16, LV_FLEX_ALIGN_END);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    value_ = label(r, &plex_mono_140_semibold, color::kGreen);
    label(r, &plex_sans_46_medium, color::kMuted, "k\xCE\xA9");  // kΩ
    spacer(r);
    chip_ = label(r, &plex_sans_32_semibold, color::kGreen);
    lv_obj_set_style_pad_hor(chip_, 14, 0);
    lv_obj_set_style_pad_ver(chip_, 6, 0);
    lv_obj_set_style_radius(chip_, 8, 0);
    lv_obj_set_style_bg_opa(chip_, LV_OPA_COVER, 0);
    spacer(card_);
    wrapping_label(card_, &plex_sans_32, color::kSub,
                   "Allowed 0.5 \xE2\x80\x93 50 k\xCE\xA9. Measured continuously while the "
                   "device runs.");
}

void ElectrodesPage::show(const Snapshot& s) {
    char v[12];
    std::snprintf(v, sizeof v, "%.1f", static_cast<double>(s.impedance_ohm / 1000.0f));
    set_text(value_, v);
    const Tone tone = s.monitor.impedance_ok ? Tone::Good : Tone::Alert;
    lv_obj_set_style_text_color(value_, tone_color(tone), 0);
    lv_obj_set_style_border_color(card_, s.monitor.impedance_ok ? color::kLine : tone_border(tone), 0);
    set_text(chip_, s.monitor.impedance_ok ? "In range" : "Out of range");
    lv_obj_set_style_text_color(chip_, tone_color(tone), 0);
    lv_obj_set_style_bg_color(chip_, tone_chip(tone), 0);
}

// ---- About ----------------------------------------------------------------------

void AboutPage::create(lv_obj_t* body) {
    root = page(body);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(root, 20, 0);
    auto line = [&](const char* name, lv_color_t value_color) {
        lv_obj_t* r = row(root, 20, LV_FLEX_ALIGN_END);
        card(r);
        lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(r, 28, 0);
        lv_obj_set_style_pad_ver(r, 24, 0);
        label(r, &plex_sans_32, color::kSub, name);
        spacer(r);
        return label(r, &plex_mono_46_medium, value_color);
    };
    serial_ = line("Serial number", color::kText);
    firmware_ = line("Firmware", color::kMuted);
}

void AboutPage::show(const Snapshot& s) {
    set_text(serial_, s.serial);
    set_text(firmware_, s.firmware);
}

// ---- Brightness -------------------------------------------------------------------

void BrightnessPage::create(lv_obj_t* body, App& app) {
    root = page(body);
    lv_obj_t* c = column(root, kTouchGap);
    card(c);
    lv_obj_set_size(c, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(c, 28, 0);
    lv_obj_set_style_pad_ver(c, 20, 0);

    lv_obj_t* top = row(c, 16, LV_FLEX_ALIGN_END);
    lv_obj_set_size(top, lv_pct(100), 120);
    value_ = label(top, &plex_mono_120_semibold, color::kText);
    label(top, &plex_sans_46_medium, color::kMuted, "%");
    spacer(top);
    lv_obj_t* type = button(top, 300);
    lv_obj_set_style_text_letter_space(type, 0, 0);
    lv_label_set_text(lv_label_create(type), "Type value");
    on_click(type, [&app] { app.open_entry(Field::Brightness); });

    lv_obj_t* r = row(c, 20);
    lv_obj_set_size(r, lv_pct(100), kTouchMin);
    label(r, &plex_mono_32, color::kMuted, "0");
    // The slider sits inside padding wide enough for its knob to reach both
    // ends without leaving the row.
    lv_obj_t* track_box = box(r);
    lv_obj_set_height(track_box, kTouchMin);
    lv_obj_set_flex_grow(track_box, 1);
    lv_obj_set_style_pad_hor(track_box, 36, 0);
    slider_ = lv_slider_create(track_box);
    lv_obj_remove_style_all(slider_);
    lv_obj_set_size(slider_, lv_pct(100), 16);
    lv_obj_center(slider_);
    lv_slider_set_range(slider_, 0, 100);
    lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_, color::kLine2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider_, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_, color::kCyan, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider_, 8, LV_PART_INDICATOR);
    // Knob: 16 px track + 2 x 28 px = 72 px, with a white rim.
    lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider_, color::kCyan, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider_, color::kWhite, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider_, 5, LV_PART_KNOB);
    lv_obj_set_style_radius(slider_, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_, 28, LV_PART_KNOB);
    // The touch area is the full 116 px row, not the 16 px track (§5.2.3).
    lv_obj_set_ext_click_area(slider_, 50);
    on_event(slider_, LV_EVENT_VALUE_CHANGED, [this, &app] {
        app.controller().set(Field::Brightness, static_cast<float>(lv_slider_get_value(slider_)));
    });
    label(r, &plex_mono_32, color::kMuted, "100");

    note_ = wrapping_label(c, &plex_sans_32, color::kMuted);
}

void BrightnessPage::show(const Snapshot& s) {
    const int pct = s.settings.brightness_pct;
    char v[8];
    std::snprintf(v, sizeof v, "%d", pct);
    set_text(value_, v);
    // Do not move the knob under the user's finger.
    if (!lv_obj_has_state(slider_, LV_STATE_PRESSED)) {
        lv_slider_set_value(slider_, pct, LV_ANIM_OFF);
    }
    // 0-100 % spans 100-500 cd/m² (UI mockup), so 0 % is dim, never dark.
    char note[96];
    std::snprintf(note, sizeof note,
                  "About %d cd/m\xC2\xB2. The scale spans 100 \xE2\x80\x93 500 cd/m\xC2\xB2, so 0 %% "
                  "is dim, never dark.",
                  100 + pct * 4);
    set_text(note_, note);
}

// ---- Option choice (alarm volume) ----------------------------------------------

void ChoicePage::create(lv_obj_t* body, App& app) {
    root = page(body);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(root, kTouchGap, 0);
    for (int i = 0; i < kVolumeCount; ++i) {
        const auto v = static_cast<Volume>(i);
        lv_obj_t* b = button(root, kMenuTileW);
        lv_obj_set_height(b, kTileH);
        lv_obj_set_style_pad_hor(b, 28, 0);
        lv_obj_set_style_text_letter_space(b, 0, 0);
        lv_obj_set_style_text_font(b, &plex_sans_46_semibold, 0);
        lv_obj_set_style_text_color(b, color::kText, 0);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* name = lv_label_create(b);
        lv_label_set_text(name, ui::name(v));
        lv_obj_set_flex_grow(name, 1);
        checks_[i] = icon(b, Icon::Check, color::kCyan, 40);
        options_[i] = b;
        on_click(b, [v, &app] { app.controller().set_volume(v); });
    }
}

void ChoicePage::show(const Snapshot& s) {
    for (int i = 0; i < kVolumeCount; ++i) {
        const bool on = static_cast<Volume>(i) == s.settings.volume;
        lv_obj_set_style_bg_color(options_[i], on ? color::kCyanPale : color::kPanel, 0);
        lv_obj_set_style_border_color(options_[i], on ? color::kCyan : color::kLine2, 0);
        lv_obj_set_style_border_width(options_[i], on ? 3 : 1, 0);
        set_visible(checks_[i], on);
    }
}

// ---- Latched fault ------------------------------------------------------------------

void FaultPage::create(lv_obj_t* body) {
    root = page(body);
    lv_obj_t* c = row(root, 32, LV_FLEX_ALIGN_START);
    lv_obj_set_size(c, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(c, 36, 0);
    lv_obj_set_style_pad_ver(c, 32, 0);
    lv_obj_set_style_radius(c, kRadius, 0);
    lv_obj_set_style_bg_color(c, lv_color_hex(0xFDF0EF), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0xD9716A), 0);
    lv_obj_set_style_border_width(c, 2, 0);

    icon(c, Icon::Warning, color::kRed, 72);
    lv_obj_t* text = column(c, 16);
    lv_obj_set_height(text, lv_pct(100));
    lv_obj_set_flex_grow(text, 1);
    wrapping_label(text, &plex_sans_77_bold, color::kRed, "Stimulation stopped");
    cause_ = wrapping_label(text, &plex_sans_46_medium, lv_color_hex(0x7A1C16));
    detail_ = wrapping_label(text, &plex_sans_32, lv_color_hex(0x4A2522));
    spacer(text);
    wrapping_label(text, &plex_sans_32, lv_color_hex(0x7A4A45),
                   "Latched until acknowledged. Current is never reapplied on its own.");
}

void FaultPage::show(const Snapshot& s) {
    const FaultText f = present_fault(s.fault_cause, s.impedance_ohm);
    set_text(cause_, f.cause);
    set_text(detail_, f.detail);
}

}  // namespace ui
