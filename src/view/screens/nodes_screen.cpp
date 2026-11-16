/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "nodes_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

namespace screen {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kDot = "  \xC2\xB7  ";   // spaced middle dot
constexpr const char* kStar = "\xE2\x98\x85";  // ★

std::string relative_time(long seconds_ago) {
    if (seconds_ago < 60) return "just now";
    if (seconds_ago < 3600) return std::to_string(seconds_ago / 60) + "m ago";
    if (seconds_ago < 86400) return std::to_string(seconds_ago / 3600) + "h ago";
    return std::to_string(seconds_ago / 86400) + "d ago";
}

std::string hops_text(int hops) {
    if (hops < 0) return "";
    if (hops == 0) return "direct";
    return std::to_string(hops) + " hops";
}

double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double dlat = (lat2 - lat1) * kPi / 180.0;
    const double dlon = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(lat1 * kPi / 180.0) * std::cos(lat2 * kPi / 180.0) *
                     std::sin(dlon / 2) * std::sin(dlon / 2);
    return 6371.0 * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

std::string distance_text(double km) {
    char buf[24];
    if (km < 1.0) std::snprintf(buf, sizeof(buf), "%d m", static_cast<int>(km * 1000));
    else std::snprintf(buf, sizeof(buf), "%.1f km", km);
    return buf;
}

lv_obj_t* make_signal_bar(lv_obj_t* parent, double snr, const view::ThemePalette& colors) {
    auto* bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 34, 7);
    lv_bar_set_range(bar, 0, 100);
    int pct = static_cast<int>((snr + 20.0) / 30.0 * 100.0);
    if (pct < 6) pct = 6;
    if (pct > 100) pct = 100;
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, colors.border, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, colors.primary, LV_PART_INDICATOR);
    return bar;
}

const char* filter_name(int filter) {
    switch (filter) {
        case 1: return "Favorites";
        case 2: return "Active";
        default: return "All nodes";
    }
}

}  // namespace

NodesScreen::NodesScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void NodesScreen::build_content(lv_obj_t* content) {
    content_area_ = content;
    list_ = lv_obj_create(content);
    lv_obj_remove_style_all(list_);
    lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_, 2, 0);
    lv_obj_set_style_pad_all(list_, 4, 0);
    lv_obj_clear_flag(list_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(list_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    rebuild();
    lv_subject_add_observer_obj(view_model().nodes_version_subject(), on_change, list_, this);
}

void NodesScreen::on_change(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<NodesScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->rebuild();
    }
}

void NodesScreen::rebuild_display() {
    display_.clear();
    const long now = static_cast<long>(std::time(nullptr));
    for (const auto& n : view_model().nodes()) {
        if (!n.isSelf) {
            if (filter_ == 1 && !n.favorite) continue;
            if (filter_ == 2 && (n.lastHeard <= 0 || now - n.lastHeard > 3600)) continue;
        }
        display_.push_back(n);
    }
    std::sort(display_.begin(), display_.end(),
              [](const viewmodel::NodeInfo& a, const viewmodel::NodeInfo& b) {
                  if (a.isSelf != b.isSelf) return a.isSelf;        // self first
                  if (a.favorite != b.favorite) return a.favorite;  // favourites next
                  return a.lastHeard > b.lastHeard;                 // most recent first
              });
    if (selected_ < 0) selected_ = 0;
    if (selected_ >= static_cast<int>(display_.size())) selected_ = static_cast<int>(display_.size()) - 1;
    if (selected_ < 0) selected_ = 0;
}

void NodesScreen::rebuild() {
    rebuild_display();
    lv_obj_clean(list_);
    const lv_font_t* name_font = assets().load_standard_font(14);
    if (!name_font) name_font = &lv_font_montserrat_14;
    const lv_font_t* detail_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    if (!detail_font) detail_font = &lv_font_montserrat_12;
    const auto colors = view::palette(view_model().is_dark_mode());

    // Filter header.
    {
        auto* header = lv_label_create(list_);
        std::string h = std::string(filter_name(filter_)) + "   \xE2\x87\xA5 filter   s " + kStar + " fav   m msg";
        lv_label_set_text(header, h.c_str());
        lv_obj_set_style_text_font(header, detail_font, 0);
        lv_obj_set_style_text_color(header, colors.text_disabled, 0);
    }

    if (display_.empty()) {
        auto* empty = lv_label_create(list_);
        lv_label_set_text(empty, filter_ == 0 ? "No nodes yet.\nStart the radio in Settings."
                                              : "No nodes match this filter.");
        lv_obj_set_style_text_font(empty, detail_font, 0);
        lv_obj_set_style_text_color(empty, colors.text_disabled, 0);
        return;
    }

    const long now = static_cast<long>(std::time(nullptr));
    lv_obj_t* selected_row = nullptr;
    int idx = 0;
    for (const auto& n : display_) {
        auto* row = lv_obj_create(list_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(row, 1, 0);
        lv_obj_set_style_pad_ver(row, 3, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        if (idx == selected_) {
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(row, colors.button, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(row, 3, 0);
            lv_obj_set_style_border_color(row, colors.primary, 0);
            selected_row = row;
        }

        std::string title;
        if (n.favorite && !n.isSelf) title += std::string(kStar) + " ";
        title += n.name.empty() ? n.id : n.name;
        if (!n.shortName.empty()) title += "  [" + n.shortName + "]";
        if (n.isSelf) title += "  (you)";
        auto* name = lv_label_create(row);
        lv_label_set_text(name, title.c_str());
        lv_obj_set_style_text_font(name, name_font, 0);
        lv_obj_set_style_text_color(name, (n.isSelf || n.favorite) ? colors.primary : colors.text, 0);

        auto* drow = lv_obj_create(row);
        lv_obj_remove_style_all(drow);
        lv_obj_set_width(drow, LV_SIZE_CONTENT);
        lv_obj_set_height(drow, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(drow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(drow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(drow, 6, 0);
        lv_obj_clear_flag(drow, LV_OBJ_FLAG_SCROLLABLE);
        if (n.hasSnr) {
            make_signal_bar(drow, n.snr, colors);
        }
        std::string detail;
        if (!n.isSelf && n.lastHeard > 0) detail += relative_time(now - n.lastHeard);
        const std::string h = hops_text(n.hops);
        if (!h.empty()) { if (!detail.empty()) detail += kDot; detail += h; }
        if (n.hasBattery) {
            char b[12];
            std::snprintf(b, sizeof(b), "%d%%", n.battery);
            if (!detail.empty()) detail += kDot;
            detail += b;
        }
        if (!detail.empty()) {
            auto* d = lv_label_create(drow);
            lv_label_set_text(d, detail.c_str());
            lv_obj_set_style_text_font(d, detail_font, 0);
            lv_obj_set_style_text_color(d, colors.text_disabled, 0);
        }
        ++idx;
    }

    lv_obj_update_layout(list_);
    if (selected_row) {
        lv_obj_scroll_to_view(selected_row, LV_ANIM_OFF);
    }
}

void NodesScreen::on_up() {
    if (detail_) {   // scroll the open detail instead of moving the list
        const int32_t room = lv_obj_get_scroll_top(detail_);
        if (room > 0) lv_obj_scroll_by(detail_, 0, room < 40 ? room : 40, LV_ANIM_OFF);
        return;
    }
    if (selected_ > 0) { --selected_; rebuild(); }
}

void NodesScreen::on_down() {
    if (detail_) {
        const int32_t room = lv_obj_get_scroll_bottom(detail_);
        if (room > 0) lv_obj_scroll_by(detail_, 0, room < 40 ? -room : -40, LV_ANIM_OFF);
        return;
    }
    if (selected_ < static_cast<int>(display_.size()) - 1) { ++selected_; rebuild(); }
}

void NodesScreen::on_enter() {
    if (!detail_) open_detail();
}

bool NodesScreen::on_back() {
    if (detail_) { close_detail(); return true; }
    return false;
}

bool NodesScreen::on_key(uint32_t key) {
    if (key == 's' || key == 'S') {   // star / favourite (F is the Up d-pad key)
        const viewmodel::NodeInfo* n = acting_node();
        if (n && !n->id.empty() && !n->isSelf) {
            const std::string id = n->id;
            const bool had_detail = detail_ != nullptr;
            view_model().toggle_favorite(id);   // bumps nodes_version -> rebuild()
            for (int i = 0; i < static_cast<int>(display_.size()); ++i) {
                if (display_[i].id == id) { selected_ = i; break; }
            }
            rebuild();
            if (had_detail) { close_detail(); detail_id_ = id; open_detail(); }
        }
        return true;
    }
    if (key == 'm' || key == 'M') {   // direct-message the acting node
        const viewmodel::NodeInfo* n = acting_node();
        if (n && !n->isSelf && !n->id.empty()) {
            view_model().set_dm_target(n->id, n->name.empty() ? n->id : n->name);
            if (detail_) close_detail();
            view_model().show_messages_page();
        }
        return true;
    }
    if (key == LV_KEY_NEXT || key == LV_KEY_PREV) {   // Tab cycles the filter
        filter_ = (filter_ + 1) % 3;
        selected_ = 0;
        if (detail_) close_detail();
        rebuild();
        return true;
    }
    return false;
}

void NodesScreen::open_detail() {
    // Resolve the node to show by stable id (it may have reordered), falling
    // back to the current selection; remember the id as the action target.
    const viewmodel::NodeInfo* np = nullptr;
    if (!detail_id_.empty()) {
        for (const auto& x : display_) if (x.id == detail_id_) { np = &x; break; }
    }
    if (!np && selected_ >= 0 && selected_ < static_cast<int>(display_.size())) {
        np = &display_[selected_];
    }
    if (!np) return;
    detail_id_ = np->id;
    const auto& n = *np;
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* title_font = assets().load_standard_font(16);
    if (!title_font) title_font = &lv_font_montserrat_16;
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    detail_ = lv_obj_create(content_area_);
    lv_obj_remove_style_all(detail_);
    lv_obj_set_size(detail_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(detail_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(detail_, colors.background, 0);
    lv_obj_set_flex_flow(detail_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(detail_, 8, 0);
    lv_obj_set_style_pad_row(detail_, 4, 0);
    lv_obj_clear_flag(detail_, LV_OBJ_FLAG_SCROLL_ELASTIC);

    std::string title;
    if (n.favorite && !n.isSelf) title += std::string(kStar) + " ";
    title += n.name.empty() ? n.id : n.name;
    if (n.isSelf) title += "  (you)";
    auto* heading = lv_label_create(detail_);
    lv_label_set_text(heading, title.c_str());
    lv_obj_set_style_text_font(heading, title_font, 0);
    lv_obj_set_style_text_color(heading, colors.primary, 0);

    const auto add_field = [&](const char* key, const std::string& value) {
        auto* row = lv_obj_create(detail_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        auto* k = lv_label_create(row);
        lv_label_set_text(k, key);
        lv_obj_set_style_text_font(k, font, 0);
        lv_obj_set_style_text_color(k, colors.text_disabled, 0);
        lv_obj_set_width(k, 78);
        auto* v = lv_label_create(row);
        lv_label_set_text(v, value.c_str());
        lv_obj_set_style_text_font(v, font, 0);
        lv_obj_set_style_text_color(v, colors.text, 0);
    };

    add_field("ID", n.id.empty() ? "-" : n.id);
    if (n.hasSnr) {
        char snr[24];
        std::snprintf(snr, sizeof(snr), "%.1f dB", n.snr);
        add_field("Signal", snr);
    }
    const long now = static_cast<long>(std::time(nullptr));
    add_field("Last heard", n.isSelf ? "this device"
                                     : (n.lastHeard > 0 ? relative_time(now - n.lastHeard) : "unknown"));
    if (n.channelUtilization >= 0) add_field("Channel busy", std::to_string(static_cast<int>(n.channelUtilization)) + "%");
    if (n.airUtilization >= 0) add_field("TX airtime", std::to_string(n.airUtilization) + "%");
    add_field("Route", hops_text(n.hops).empty() ? "unknown" : hops_text(n.hops));
    if (n.hasBattery) {
        char bat[32];
        std::snprintf(bat, sizeof(bat), "%d%%  (%.2f V)", n.battery, n.voltage);
        add_field("Battery", bat);
    } else {
        add_field("Battery", "-");
    }
    if (n.hasPos) {
        char pos[48];
        std::snprintf(pos, sizeof(pos), "%.4f, %.4f", n.lat, n.lon);
        add_field("Position", pos);
        add_field("Fix age", n.positionTime > 0 && now >= n.positionTime ? std::to_string(now-n.positionTime) + "s" + (now-n.positionTime > 120 ? " (stale)" : "") : "Unknown");
        add_field("Satellites", n.satellites < 0 ? "Unknown" : std::to_string(n.satellites));
        // Distance from our own node, if we have a position.
        const viewmodel::NodeInfo* me = nullptr;
        for (const auto& x : view_model().nodes()) {
            if (x.isSelf && x.hasPos) { me = &x; break; }
        }
        if (me && !n.isSelf) {
            add_field("Distance", distance_text(haversine_km(me->lat, me->lon, n.lat, n.lon)));
        }
    } else {
        add_field("Position", "-");
    }
    add_field("Favourite", n.isSelf ? "-" : (n.favorite ? "yes" : "no"));

    auto* hint = lv_label_create(detail_);
    lv_label_set_text(hint, n.isSelf ? "Esc: back"
                                     : "m: message   \xC2\xB7   s: favourite   \xC2\xB7   Esc: back");
    lv_obj_set_style_text_font(hint, font, 0);
    lv_obj_set_style_text_color(hint, colors.text_disabled, 0);
}

void NodesScreen::close_detail() {
    if (detail_) {
        lv_obj_delete(detail_);
        detail_ = nullptr;
    }
    detail_id_.clear();
}

const viewmodel::NodeInfo* NodesScreen::acting_node() const {
    // When the detail is open, actions target the node it shows (by id), not the
    // list selection, which may have reordered underneath an open overlay.
    if (detail_ && !detail_id_.empty()) {
        for (const auto& n : display_) if (n.id == detail_id_) return &n;
        return nullptr;
    }
    if (selected_ >= 0 && selected_ < static_cast<int>(display_.size())) {
        return &display_[selected_];
    }
    return nullptr;
}

} // namespace screen
