/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "map_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "geo.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace screen {
namespace {
using viewmodel::geo::distance_text;
using viewmodel::geo::haversine_km;

constexpr int kCx = 160;   // plot centre (content is 320 x 140)
constexpr int kCy = 70;
constexpr int kR = 60;     // outer ring radius

using viewmodel::geo::to_rad;

double bearing_rad(double lat1, double lon1, double lat2, double lon2) {
    const double dlon = to_rad(lon2 - lon1);
    const double y = std::sin(dlon) * std::cos(to_rad(lat2));
    const double x = std::cos(to_rad(lat1)) * std::sin(to_rad(lat2)) -
                     std::sin(to_rad(lat1)) * std::cos(to_rad(lat2)) * std::cos(dlon);
    return std::atan2(y, x);   // 0 = north, clockwise
}

lv_obj_t* make_ring(lv_obj_t* parent, int radius, lv_color_t color) {
    auto* ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, radius * 2, radius * 2);
    lv_obj_set_pos(ring, kCx - radius, kCy - radius);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, color, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_50, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    return ring;
}

lv_obj_t* make_dot(lv_obj_t* parent, int x, int y, int size, lv_color_t color) {
    auto* dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_pos(dot, x - size / 2, y - size / 2);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

}  // namespace

MapScreen::MapScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void MapScreen::build_content(lv_obj_t* content) {
    plot_ = lv_obj_create(content);
    lv_obj_remove_style_all(plot_);
    lv_obj_set_size(plot_, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(plot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_subject_add_observer_obj(view_model().nodes_version_subject(), on_change, plot_, this);
    lv_subject_add_observer_obj(view_model().clock_subject(), on_clock, plot_, this);
}

void MapScreen::on_change(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<MapScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->rebuild();
    }
}

void MapScreen::rebuild() {
    dots_.clear();
    you_ = nullptr;
    lv_obj_clean(plot_);
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto now = view_model().presentation_time();
    const auto fresh = [now](const viewmodel::NodeInfo& n) {
        return n.positionTime > 0 && now >= n.positionTime && now - n.positionTime <= 120;
    };
    const lv_font_t* font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_12;

    // Our own position.
    const viewmodel::NodeInfo* me = nullptr;
    for (const auto& n : view_model().nodes()) {
        if (n.isSelf && n.hasPos) { me = &n; break; }
    }
    if (!me) {
        auto* msg = lv_label_create(plot_);
        lv_label_set_text(msg, "No position fix yet.\nGo outside with a sky view\nand give the GPS a minute.");
        lv_obj_set_style_text_font(msg, font, 0);
        lv_obj_set_style_text_color(msg, colors.text_disabled, 0);
        lv_obj_center(msg);
        return;
    }

    // Positioned peers + the farthest distance (for scaling).
    struct Peer { const viewmodel::NodeInfo* n; double dist; double bearing; };
    std::vector<Peer> peers;
    double maxd = 0.0;
    for (const auto& n : view_model().nodes()) {
        if (n.isSelf || !n.hasPos) continue;
        const double d = haversine_km(me->lat, me->lon, n.lat, n.lon);
        const double b = bearing_rad(me->lat, me->lon, n.lat, n.lon);
        peers.push_back({&n, d, b});
        if (d > maxd) maxd = d;
    }
    if (maxd <= 0.0) maxd = 1.0;

    make_ring(plot_, kR, colors.border);
    make_ring(plot_, kR / 2, colors.border);

    // Peer dots.
    for (const auto& p : peers) {
        const double r = (p.dist / maxd) * kR;
        const int x = kCx + static_cast<int>(std::sin(p.bearing) * r);
        const int y = kCy - static_cast<int>(std::cos(p.bearing) * r);
        dots_.push_back({make_dot(plot_, x, y, 9, fresh(*p.n) ? colors.primary : colors.text_disabled),
                         p.n->positionTime, fresh(*p.n), false});
        lv_obj_set_style_border_width(dots_.back().object, 2, 0);
        lv_obj_set_style_border_color(dots_.back().object, colors.text, 0);
        lv_obj_set_style_bg_opa(dots_.back().object, fresh(*p.n) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        auto* label = lv_label_create(plot_);
        const std::string tag = p.n->shortName.empty() ? p.n->name : p.n->shortName;
        lv_label_set_text(label, tag.c_str());
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, colors.text, 0);
        lv_obj_set_pos(label, x + 7, y - 7);
        lv_obj_set_width(label, std::min(100, view::kScreenWidth - 6 - (x + 7)));
        lv_obj_set_height(label, lv_font_get_line_height(font));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }

    // You, at the centre.
    dots_.push_back({make_dot(plot_, kCx, kCy, 11, fresh(*me) ? colors.primary : colors.text_disabled),
                     me->positionTime, fresh(*me), true});
    auto* you = lv_label_create(plot_);
    you_ = you;
    lv_label_set_text(you, fresh(*me) ? "you" : "you (last known)");
    lv_obj_set_style_text_font(you, font, 0);
    lv_obj_set_style_text_color(you, colors.text, 0);
    lv_obj_set_pos(you, kCx + 8, kCy + 4);

    // Scale note.
    auto* scale = lv_label_create(plot_);
    lv_label_set_text(scale, ("Filled=fresh / ring: " + distance_text(maxd)).c_str());
    lv_obj_set_style_text_font(scale, font, 0);
    lv_obj_set_style_text_color(scale, colors.text_disabled, 0);
    lv_obj_set_pos(scale, 6, 2);

    if (peers.empty()) {
        auto* none = lv_label_create(plot_);
        lv_label_set_text(none, "No other nodes have shared a position.");
        lv_obj_set_style_text_font(none, font, 0);
        lv_obj_set_style_text_color(none, colors.text_disabled, 0);
        lv_obj_align(none, LV_ALIGN_BOTTOM_MID, 0, -2);
    }
}

void MapScreen::on_clock(lv_observer_t *observer, lv_subject_t *) {
    auto *self = static_cast<MapScreen *>(lv_observer_get_user_data(observer));
    if (self)
        self->refresh_freshness();
}

void MapScreen::refresh_freshness() {
    const auto colors = view::palette(view_model().is_dark_mode());
    for (auto &dot : dots_) {
        const bool fresh = viewmodel::geo::fresh(dot.stamp, view_model().presentation_time());
        if (fresh == dot.fresh)
            continue;
        dot.fresh = fresh;
        lv_obj_set_style_bg_color(dot.object, fresh ? colors.primary : colors.text_disabled, 0);
        if (!dot.self) lv_obj_set_style_bg_opa(dot.object, fresh ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (dot.self && you_)
            lv_label_set_text(you_, fresh ? "you" : "you (last known)");
    }
}

} // namespace screen
