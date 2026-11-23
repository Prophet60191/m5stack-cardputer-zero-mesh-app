/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "nodes_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "list_selection.h"
#include "geo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <set>

namespace screen {
namespace {
using viewmodel::geo::distance_text;
using viewmodel::geo::haversine_km;

constexpr const char* kDot = "  \xC2\xB7  ";   // spaced middle dot
constexpr const char* kStar = "\xE2\x98\x85";  // ★

std::string relative_time(long seconds_ago) {
    if (seconds_ago < 0)
        return "unknown";
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

std::string node_summary(const viewmodel::NodeInfo &n, long now) {
    std::string detail;
    if (!n.isSelf && n.lastHeard > 0)
        detail += relative_time(now - n.lastHeard);
    const auto hops = hops_text(n.hops);
    if (!hops.empty()) {
        if (!detail.empty())
            detail += kDot;
        detail += hops;
    }
    if (n.hasBattery) {
        if (!detail.empty())
            detail += kDot;
        detail += std::to_string(n.battery) + "%";
    }
    return detail;
}
void set_text(lv_obj_t *label, const std::string &text) {
    if (text != lv_label_get_text(label))
        lv_label_set_text(label, text.c_str());
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
    view::widgets::style_list_scrollbar(list_, view::palette(view_model().is_dark_mode()));
    lv_subject_add_observer_obj(view_model().nodes_version_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().clock_subject(), on_clock, list_, this);
}

void NodesScreen::on_change(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<NodesScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->rebuild();
        self->refresh_detail();
    }
}

void NodesScreen::rebuild_display() {
    display_.clear();
    const long now = view_model().presentation_time();
    for (const auto& n : view_model().nodes()) {
        if (!n.isSelf) {
            if (filter_ == 1 && !n.favorite) continue;
            if (filter_ == 2 && (n.lastHeard <= 0 || now < n.lastHeard || now - n.lastHeard > 3600))
                continue;
        }
        display_.push_back(n);
    }
    std::sort(display_.begin(), display_.end(),
              [](const viewmodel::NodeInfo& a, const viewmodel::NodeInfo& b) {
                  if (a.isSelf != b.isSelf) return a.isSelf;        // self first
                  if (a.favorite != b.favorite) return a.favorite;  // favourites next
                  return a.lastHeard > b.lastHeard;                 // most recent first
              });
    if (!selected_id_.empty()) {
        const auto it = std::find_if(display_.begin(), display_.end(),
                                     [&](const auto &n) { return n.id == selected_id_; });
        selected_ = it == display_.end() ? -1 : static_cast<int>(it - display_.begin());
    }
    if (selected_ >= static_cast<int>(display_.size())) selected_ = static_cast<int>(display_.size()) - 1;
    selected_id_ = selected_ >= 0 ? display_[selected_].id : "";
}

void NodesScreen::rebuild() {
    const int previous_scroll = lv_obj_get_scroll_y(list_);
    rebuild_display();
    list_rows_.clear();
    time_labels_.clear();
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

    const long now = view_model().presentation_time();
    lv_obj_t* selected_row = nullptr;
    int idx = 0;
    for (const auto& n : display_) {
        auto* row = lv_obj_create(list_);
        list_rows_.push_back(row);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(row, 1, 0);
        lv_obj_set_style_pad_ver(row, 3, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        view::widgets::style_list_row(row, colors, idx == selected_);
        if (idx == selected_)
            selected_row = row;

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
        auto *d = lv_label_create(drow);
        time_labels_.push_back(d);
        const auto detail = node_summary(n, now);
        lv_label_set_text(d, detail.c_str());
        if (detail.empty())
            lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(d, detail_font, 0);
        lv_obj_set_style_text_color(d, colors.text_disabled, 0);
        ++idx;
    }

    lv_obj_update_layout(list_);
    lv_obj_scroll_to_y(list_, previous_scroll, LV_ANIM_OFF);
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
    if (selected_ > 0)
        select_node(selected_ - 1);
}

void NodesScreen::on_down() {
    if (detail_) {
        const int32_t room = lv_obj_get_scroll_bottom(detail_);
        if (room > 0) lv_obj_scroll_by(detail_, 0, room < 40 ? -room : -40, LV_ANIM_OFF);
        return;
    }
    if (selected_ < static_cast<int>(display_.size()) - 1)
        select_node(selected_ + 1);
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
            view_model().toggle_favorite(id);
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
        selected_id_.clear();
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
    view::widgets::style_list_scrollbar(detail_, colors);

    detail_heading_ = lv_label_create(detail_);
    lv_obj_set_style_text_font(detail_heading_, title_font, 0);
    lv_obj_set_style_text_color(detail_heading_, colors.primary, 0);
    for (const char *key : {"ID", "Signal", "Last heard", "Channel busy", "TX airtime", "Route", "Battery",
                            "Position", "Fix age", "Satellites", "Distance", "Favourite"}) {
        auto *row = lv_obj_create(detail_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        auto *k = lv_label_create(row);
        lv_label_set_text(k, key);
        lv_obj_set_style_text_font(k, font, 0);
        lv_obj_set_style_text_color(k, colors.text_disabled, 0);
        lv_obj_set_width(k, 78);
        auto *v = lv_label_create(row);
        lv_obj_set_style_text_font(v, font, 0);
        lv_obj_set_style_text_color(v, colors.text, 0);
        detail_fields_[key] = v;
    }
    detail_hint_ = lv_label_create(detail_);
    lv_obj_set_style_text_font(detail_hint_, font, 0);
    lv_obj_set_style_text_color(detail_hint_, colors.text_disabled, 0);
    refresh_detail();
}

void NodesScreen::refresh_detail() {
    if (!detail_)
        return;
    const auto it = std::find_if(view_model().nodes().begin(), view_model().nodes().end(),
                                 [&](const auto &n) { return n.id == detail_id_; });
    if (it == view_model().nodes().end()) {
        set_text(detail_heading_, "Node unavailable");
        lv_obj_add_flag(detail_hint_, LV_OBJ_FLAG_HIDDEN);
        for (const auto &field : detail_fields_)
            lv_obj_add_flag(lv_obj_get_parent(field.second), LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const auto &n = *it;
    const auto now = view_model().presentation_time();
    const auto field = [&](const std::string &key, const std::string &text, bool visible = true) {
        auto *value = detail_fields_.at(key);
        set_text(value, text);
        auto *row = lv_obj_get_parent(value);
        if (visible)
            lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    };
    set_text(detail_heading_, std::string(n.favorite && !n.isSelf ? kStar : "") +
                                  (n.favorite && !n.isSelf ? " " : "") + (n.name.empty() ? n.id : n.name) +
                                  (n.isSelf ? "  (you)" : ""));
    field("ID", n.id.empty() ? "-" : n.id);
    char value[64];
    std::snprintf(value, sizeof(value), "%.1f dB", n.snr);
    field("Signal", value, n.hasSnr);
    field("Last heard", n.isSelf          ? "this device"
                        : n.lastHeard > 0 ? relative_time(now - n.lastHeard)
                                          : "unknown");
    field("Channel busy", std::to_string(static_cast<int>(n.channelUtilization)) + "%",
          n.channelUtilization >= 0);
    field("TX airtime", std::to_string(n.airUtilization) + "%", n.airUtilization >= 0);
    field("Route", hops_text(n.hops).empty() ? "unknown" : hops_text(n.hops));
    std::snprintf(value, sizeof(value), "%d%%  (%.2f V)", n.battery, n.voltage);
    field("Battery", n.hasBattery ? value : "-");
    std::snprintf(value, sizeof(value), "%.4f, %.4f", n.lat, n.lon);
    field("Position", n.hasPos ? value : "-");
    field("Fix age",
          n.positionTime > 0 && now >= n.positionTime
              ? std::to_string(now - n.positionTime) + "s" + (now - n.positionTime > 120 ? " (stale)" : "")
              : "Unknown",
          n.hasPos);
    field("Satellites", n.satellites < 0 ? "Unknown" : std::to_string(n.satellites), n.hasPos);
    const auto me = std::find_if(view_model().nodes().begin(), view_model().nodes().end(),
                                 [](const auto &x) { return x.isSelf && x.hasPos; });
    const bool distance = n.hasPos && !n.isSelf && me != view_model().nodes().end();
    field("Distance", distance ? distance_text(haversine_km(me->lat, me->lon, n.lat, n.lon)) : "-", distance);
    field("Favourite", n.isSelf ? "-" : n.favorite ? "yes" : "no");
    if (n.isSelf) lv_obj_add_flag(detail_hint_, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_remove_flag(detail_hint_, LV_OBJ_FLAG_HIDDEN);
        set_text(detail_hint_, "m: Message   \xC2\xB7   s: Favourite");
    }
}

void NodesScreen::on_clock(lv_observer_t *observer, lv_subject_t *) {
    auto *self = static_cast<NodesScreen *>(lv_observer_get_user_data(observer));
    if (self)
        self->refresh_times();
}
void NodesScreen::refresh_times() {
    const auto now = view_model().presentation_time();
    if (filter_ == 2) {
        std::set<std::string> expected, shown;
        for (const auto &n : view_model().nodes())
            if (n.isSelf || (n.lastHeard > 0 && now >= n.lastHeard && now - n.lastHeard <= 3600))
                expected.insert(n.id);
        for (const auto &n : display_)
            shown.insert(n.id);
        if (expected != shown)
            rebuild();
    }
    for (size_t i = 0; i < display_.size() && i < time_labels_.size(); ++i) {
        const auto text = node_summary(display_[i], now);
        set_text(time_labels_[i], text);
        if (text.empty())
            lv_obj_add_flag(time_labels_[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_remove_flag(time_labels_[i], LV_OBJ_FLAG_HIDDEN);
    }
    refresh_detail();
}
void NodesScreen::select_node(int index) {
    view::widgets::select_list_row(list_rows_, selected_, index, view::palette(view_model().is_dark_mode()));
    selected_ = index;
    selected_id_ = display_[index].id;
}

void NodesScreen::close_detail() {
    if (detail_) {
        lv_obj_delete(detail_);
        detail_ = nullptr;
    }
    detail_id_.clear();
    detail_fields_.clear();
    detail_heading_ = detail_hint_ = nullptr;
}

const viewmodel::NodeInfo* NodesScreen::acting_node() const {
    // When the detail is open, actions target the node it shows (by id), not the
    // list selection, which may have reordered underneath an open overlay.
    if (detail_ && !detail_id_.empty()) {
        for (const auto &n : view_model().nodes())
            if (n.id == detail_id_)
                return &n;
        return nullptr;
    }
    if (selected_ >= 0 && selected_ < static_cast<int>(display_.size())) {
        return &display_[selected_];
    }
    return nullptr;
}

} // namespace screen
