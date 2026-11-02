// Building blocks shared by every page: palette, sizes and widget helpers.
// Internal to the ui component.

#ifndef NMT_UI_WIDGETS_HPP
#define NMT_UI_WIDGETS_HPP

#include <functional>

#include "lvgl.h"
#include "ui/fonts.hpp"
#include "ui/monitor_model.hpp"

namespace ui::w {

// Palette, from the UI mockup.
namespace color {
inline const lv_color_t kScreen = lv_color_hex(0xFFFFFF);
inline const lv_color_t kBar = lv_color_hex(0xF3F5F7);
inline const lv_color_t kPanel = lv_color_hex(0xF7F9FB);
inline const lv_color_t kLine = lv_color_hex(0xD3DAE1);
inline const lv_color_t kLine2 = lv_color_hex(0xC3CCD5);
inline const lv_color_t kTrack = lv_color_hex(0xE9EDF1);
inline const lv_color_t kText = lv_color_hex(0x111820);
inline const lv_color_t kSub = lv_color_hex(0x3D4A57);
inline const lv_color_t kSoft = lv_color_hex(0x1F2A35);
inline const lv_color_t kMuted = lv_color_hex(0x5A6875);
inline const lv_color_t kDim = lv_color_hex(0x5F6C79);
inline const lv_color_t kGrey = lv_color_hex(0x77838F);
inline const lv_color_t kCyan = lv_color_hex(0x0B72A8);
inline const lv_color_t kCyanPale = lv_color_hex(0xE6F1F8);
inline const lv_color_t kGreen = lv_color_hex(0x1B7A48);
inline const lv_color_t kAmber = lv_color_hex(0x9A5B00);
inline const lv_color_t kRed = lv_color_hex(0xB3261E);
inline const lv_color_t kRedDark = lv_color_hex(0x8F1E18);
inline const lv_color_t kWhite = lv_color_hex(0xFFFFFF);
inline const lv_color_t kOffFill = lv_color_hex(0xF0F2F4);
inline const lv_color_t kOffLine = lv_color_hex(0xE3E8ED);
inline const lv_color_t kOffText = lv_color_hex(0xA9B3BD);
}  // namespace color

// ADR-0008 / §5.2.3: touch targets at least 116 px (10 mm), 58 px (5 mm) apart.
inline constexpr int32_t kTouchMin = 116;
inline constexpr int32_t kTouchGap = 58;

inline constexpr int32_t kScreenW = 1280;
inline constexpr int32_t kHeaderH = 88;
inline constexpr int32_t kBodyH = 484;
inline constexpr int32_t kFooterH = 148;
inline constexpr int32_t kSide = 32;
inline constexpr int32_t kRadius = 12;
// Menu and settings tiles.
inline constexpr int32_t kTileH = 189;

lv_color_t tone_color(Tone t);
lv_color_t tone_border(Tone t);
lv_color_t tone_chip(Tone t);

// A plain container with no theme styling and no scrolling. Every box on the
// screen starts from this, so its look is exactly what the code sets.
lv_obj_t* box(lv_obj_t* parent);
lv_obj_t* row(lv_obj_t* parent, int32_t gap, lv_flex_align_t cross = LV_FLEX_ALIGN_CENTER);
lv_obj_t* column(lv_obj_t* parent, int32_t gap);
// Takes up the remaining space in a flex row or column.
void spacer(lv_obj_t* parent);
// A full-size page inside the body, padded like the mockup.
lv_obj_t* page(lv_obj_t* body, int32_t pad_ver = 24);
void card(lv_obj_t* o, int32_t border = 1);

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t c, const char* text = "");
// Sets text only when it changed, so LVGL does not repaint an unchanged label.
void set_text(lv_obj_t* l, const char* text);
void set_visible(lv_obj_t* o, bool visible);
void set_enabled(lv_obj_t* o, bool enabled);

// Line icons, drawn with LVGL lines because SVG is not available (ADR-0008).
enum class Icon { ChevronRight, ChevronLeft, Menu, Check, Delete, Warning };
lv_obj_t* icon(lv_obj_t* parent, Icon i, lv_color_t c, int32_t size = 32);

// A touch button: 116 px tall, white, 32 px semibold text, pressed and
// disabled looks. Lay out its content as a flex row.
lv_obj_t* button(lv_obj_t* parent, int32_t width);

// Runs `fn` when `obj` is clicked. The callable lives as long as the object.
void on_click(lv_obj_t* obj, std::function<void()> fn);
void on_event(lv_obj_t* obj, lv_event_code_t code, std::function<void()> fn);

}  // namespace ui::w

#endif  // NMT_UI_WIDGETS_HPP
