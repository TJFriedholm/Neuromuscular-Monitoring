#include "ui/app.hpp"

#include "pages.hpp"
#include "widgets.hpp"

namespace ui {

using namespace w;

struct App::Parts {
    // Header
    lv_obj_t* title;
    lv_obj_t* electrodes_dot;
    lv_obj_t* electrodes;
    lv_obj_t* next;
    lv_obj_t* state;

    // Body
    MonitorPage monitor;
    SignalPage signal;
    TilesPage tiles;
    ElectrodesPage electrodes_page;
    AboutPage about;
    BrightnessPage brightness;
    ChoicePage choice;
    EntryPage entry;
    FaultPage fault;

    // Footer: one bar per kind of page, only one visible.
    lv_obj_t* home_bar;  // Monitor: START, STOP, Menu
    lv_obj_t* start;
    lv_obj_t* start_label;
    lv_obj_t* home_stop;
    lv_obj_t* back_bar;  // other pages: Back, and STOP while running
    lv_obj_t* back_label;
    lv_obj_t* back_stop;
    lv_obj_t* entry_bar;  // keypad: Cancel, Enter
    lv_obj_t* enter;
    lv_obj_t* fault_bar;  // latched fault: Acknowledge only
};

App::App() : parts_(std::make_unique<Parts>()) {}
App::~App() = default;

namespace {

lv_obj_t* bar(lv_obj_t* parent, int32_t height, lv_border_side_t side) {
    lv_obj_t* b = row(parent, kTouchGap);
    lv_obj_set_size(b, kScreenW, height);
    lv_obj_set_style_pad_hor(b, kSide, 0);
    lv_obj_set_style_bg_color(b, color::kBar, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(b, side, 0);
    lv_obj_set_style_border_color(b, color::kLine, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    return b;
}

// A footer row filling the footer, so the four kinds can be swapped.
lv_obj_t* footer_row(lv_obj_t* footer) {
    lv_obj_t* r = row(footer, kTouchGap);
    lv_obj_set_size(r, lv_pct(100), lv_pct(100));
    return r;
}

lv_obj_t* stop_button(lv_obj_t* parent, Controller& c) {
    lv_obj_t* b = button(parent, 300);
    lv_obj_set_style_bg_color(b, color::kRed, 0);
    lv_obj_set_style_border_color(b, color::kRedDark, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_text_color(b, color::kWhite, 0);
    lv_obj_set_style_bg_color(b, color::kRedDark, LV_STATE_PRESSED);
    lv_label_set_text(lv_label_create(b), "STOP");
    on_click(b, [&c] { c.stop(); });
    return b;
}

}  // namespace

void App::create(lv_obj_t* screen, Controller& c) {
    controller_ = &c;
    Parts& p = *parts_;

    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, color::kScreen, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

    // ---- Header: page title, electrodes, next measurement, device state -----
    lv_obj_t* header = bar(screen, kHeaderH, LV_BORDER_SIDE_BOTTOM);
    lv_obj_set_style_pad_column(header, 40, 0);
    p.title = label(header, &plex_sans_32_semibold, color::kText);
    spacer(header);
    lv_obj_t* elec = row(header, 12);
    lv_obj_set_size(elec, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    p.electrodes_dot = box(elec);
    lv_obj_set_size(p.electrodes_dot, 16, 16);
    lv_obj_set_style_radius(p.electrodes_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(p.electrodes_dot, LV_OPA_COVER, 0);
    p.electrodes = label(elec, &plex_sans_32, color::kGreen);
    p.next = label(header, &plex_mono_32, color::kSub);
    p.state = label(header, &plex_sans_46_semibold, color::kMuted);

    // ---- Body: every page built once, one shown at a time -------------------
    lv_obj_t* body = box(screen);
    lv_obj_set_size(body, kScreenW, kBodyH);
    p.monitor.create(body, *this);
    p.signal.create(body);
    p.tiles.create(body, *this);
    p.electrodes_page.create(body);
    p.about.create(body);
    p.brightness.create(body, *this);
    p.choice.create(body, *this);
    p.entry.create(body, *this);
    p.fault.create(body);
    // Pages all fill the body, on top of one another; show() makes exactly
    // one visible.

    // ---- Footer -------------------------------------------------------------
    lv_obj_t* footer = bar(screen, kFooterH, LV_BORDER_SIDE_TOP);

    p.home_bar = footer_row(footer);
    p.start = button(p.home_bar, 300);
    lv_obj_set_style_bg_color(p.start, color::kGreen, 0);
    lv_obj_set_style_border_width(p.start, 0, 0);
    lv_obj_set_style_text_color(p.start, color::kWhite, 0);
    lv_obj_set_style_bg_color(p.start, lv_color_hex(0x15603A), LV_STATE_PRESSED);
    // While running, START reads "Running" in a pale green rather than grey.
    lv_obj_set_style_bg_color(p.start, lv_color_hex(0xE3F3EA), LV_STATE_DISABLED);
    lv_obj_set_style_border_color(p.start, lv_color_hex(0x7CC49C), LV_STATE_DISABLED);
    lv_obj_set_style_border_width(p.start, 2, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(p.start, color::kGreen, LV_STATE_DISABLED);
    p.start_label = lv_label_create(p.start);
    on_click(p.start, [&c] { c.start(); });
    p.home_stop = stop_button(p.home_bar, c);
    spacer(p.home_bar);
    lv_obj_t* menu = button(p.home_bar, 240);
    icon(menu, Icon::Menu, color::kSoft);
    lv_label_set_text(lv_label_create(menu), "Menu");
    on_click(menu, [this] { go(Page::Menu); });

    p.back_bar = footer_row(footer);
    lv_obj_t* back = button(p.back_bar, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(back, 300, 0);
    lv_obj_set_style_pad_left(back, 24, 0);
    lv_obj_set_style_pad_right(back, 32, 0);
    lv_obj_set_style_pad_column(back, 12, 0);
    lv_obj_set_style_text_letter_space(back, 0, 0);
    icon(back, Icon::ChevronLeft, color::kSoft);
    p.back_label = lv_label_create(back);
    on_click(back, [this] {
        go(page_ == Page::Entry ? entry_return_ : parent_of(page_));
    });
    spacer(p.back_bar);
    p.back_stop = stop_button(p.back_bar, c);

    p.entry_bar = footer_row(footer);
    lv_obj_t* cancel = button(p.entry_bar, 240);
    lv_obj_set_style_text_letter_space(cancel, 0, 0);
    lv_label_set_text(lv_label_create(cancel), "Cancel");
    on_click(cancel, [this] { go(entry_return_); });
    spacer(p.entry_bar);
    p.enter = button(p.entry_bar, 300);
    lv_obj_set_style_text_letter_space(p.enter, 0, 0);
    lv_obj_set_style_bg_color(p.enter, color::kCyan, 0);
    lv_obj_set_style_border_width(p.enter, 0, 0);
    lv_obj_set_style_text_color(p.enter, color::kWhite, 0);
    lv_label_set_text(lv_label_create(p.enter), "Enter");
    on_click(p.enter, [this] {
        const EntryCheck chk = check(entry_);
        if (chk.valid) {
            controller_->set(entry_.field, chk.value);
            go(entry_return_);
        }
    });

    p.fault_bar = footer_row(footer);
    lv_obj_t* ack = button(p.fault_bar, 460);
    lv_obj_set_style_bg_color(ack, color::kRed, 0);
    lv_obj_set_style_border_color(ack, color::kRedDark, 0);
    lv_obj_set_style_border_width(ack, 2, 0);
    lv_obj_set_style_text_color(ack, color::kWhite, 0);
    lv_obj_set_style_bg_color(ack, color::kRedDark, LV_STATE_PRESSED);
    lv_label_set_text(lv_label_create(ack), "Acknowledge fault");
    on_click(ack, [this] {
        controller_->acknowledge_fault();
        go(Page::Monitor);
    });
}

void App::go(Page p) {
    page_ = p;
}

void App::open_entry(Field f) {
    entry_return_ = page_;
    entry_ = Entry{};
    entry_.field = f;
    page_ = Page::Entry;
}

void App::activate(const TileTarget& t) {
    switch (t.kind) {
        case TileTarget::Kind::OpenPage: go(t.page); break;
        case TileTarget::Kind::OpenEntry: open_entry(t.field); break;
        case TileTarget::Kind::OpenChoice: go(Page::Choice); break;
        case TileTarget::Kind::Recalibrate:
            controller_->recalibrate();
            go(Page::Monitor);
            break;
        case TileTarget::Kind::None: break;
    }
}

void App::show(const Snapshot& s) {
    Parts& p = *parts_;
    const MonitorView v = present(s.monitor);
    const bool fault = s.monitor.state == safety::State::Fault;
    const bool running = v.stop_enabled;

    // Header
    const char* title = fault                 ? title_of(Page::Monitor)
                        : page_ == Page::Entry ? spec(entry_.field).title
                                               : title_of(page_);
    set_text(p.title, title);
    set_text(p.state, v.state);
    lv_obj_set_style_text_color(p.state, tone_color(v.state_tone), 0);
    set_text(p.electrodes, v.electrodes);
    lv_obj_set_style_text_color(p.electrodes, tone_color(v.electrodes_tone), 0);
    lv_obj_set_style_bg_color(p.electrodes_dot, tone_color(v.electrodes_tone), 0);
    set_text(p.next, v.next);
    set_visible(p.next, v.next[0] != '\0');

    // Body. A latched fault replaces whatever page was open.
    const Page shown = fault ? Page::Monitor : page_;
    const bool tiles = shown == Page::Menu || shown == Page::Measurement ||
                       shown == Page::Stimulation || shown == Page::Alarms ||
                       shown == Page::Device;
    set_visible(p.fault.root, fault);
    set_visible(p.monitor.root, !fault && shown == Page::Monitor);
    set_visible(p.signal.root, !fault && shown == Page::Signal);
    set_visible(p.tiles.root, !fault && tiles);
    set_visible(p.electrodes_page.root, !fault && shown == Page::Electrodes);
    set_visible(p.about.root, !fault && shown == Page::About);
    set_visible(p.brightness.root, !fault && shown == Page::Brightness);
    set_visible(p.choice.root, !fault && shown == Page::Choice);
    set_visible(p.entry.root, !fault && shown == Page::Entry);

    if (fault) {
        p.fault.show(s);
    } else {
        switch (shown) {
            case Page::Monitor: p.monitor.show(v); break;
            case Page::Signal: p.signal.show(v); break;
            case Page::Electrodes: p.electrodes_page.show(s); break;
            case Page::About: p.about.show(s); break;
            case Page::Brightness: p.brightness.show(s); break;
            case Page::Choice: p.choice.show(s); break;
            case Page::Entry: p.entry.show(s, entry_); break;
            default:
                if (tiles) {
                    p.tiles.show(shown, s);
                }
                break;
        }
    }

    // Footer
    set_visible(p.fault_bar, fault);
    set_visible(p.home_bar, !fault && shown == Page::Monitor);
    set_visible(p.entry_bar, !fault && shown == Page::Entry);
    set_visible(p.back_bar, !fault && shown != Page::Monitor && shown != Page::Entry);

    set_enabled(p.start, v.start_enabled);
    set_text(p.start_label, v.start_label);
    set_enabled(p.home_stop, v.stop_enabled);
    set_visible(p.back_stop, running);
    set_text(p.back_label, title_of(parent_of(shown)));
    set_enabled(p.enter, shown == Page::Entry && check(entry_).valid);
}

void App::set_trace(const std::int32_t* samples_uv, std::size_t n) {
    parts_->monitor.set_trace(samples_uv, n);
    parts_->signal.set_trace(samples_uv, n);
}

}  // namespace ui
