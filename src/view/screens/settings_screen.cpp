/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "settings_screen.h"

#include "app_info.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <string>

namespace screen {
namespace {

const char* const kRows[] = {
    "Radio", "Region", "Modem preset", "Hop limit", "Add a channel", "Node name",
    "Share position", "Internet (MQTT)", "Auto-start radio", "Theme", "Channels", "About",
    "Quick messages", "Signal on messages", "Keep per chat", "Mute all", "Message acks",
    "Frequency slot", "First-run setup", "Transmit mode", "Channel manager", "Mesh profiles", "Diagnostics", "Save conversations", "Quiet hours",
};
constexpr int kRowCount = 25;

const std::vector<std::string> kRegions = {
    "US", "EU_868", "EU_433", "ANZ", "NZ_865", "KR", "TW", "JP", "SG_923", "TH", "MY_919", "PH_915",
};
const std::vector<std::string> kPresets = {
    "LONG_FAST", "LONG_MODERATE", "LONG_SLOW", "MEDIUM_FAST", "MEDIUM_SLOW", "SHORT_FAST", "SHORT_SLOW",
};

std::string s_of(lv_subject_t* subj) {
    const char* s = lv_subject_get_string(subj);
    return s ? s : "";
}

int index_of(const std::vector<std::string>& v, const std::string& x) {
    for (int i = 0; i < static_cast<int>(v.size()); ++i) if (v[i] == x) return i;
    return 0;
}

}  // namespace

SettingsScreen::SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SettingsScreen::build_content(lv_obj_t* content) {
    list_ = lv_obj_create(content);
    lv_obj_remove_style_all(list_);
    lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_, 1, 0);
    lv_obj_set_style_pad_all(list_, 4, 0);
    lv_obj_clear_flag(list_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(list_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    rebuild();
    lv_subject_add_observer_obj(view_model().services_subject(), services_cb, list_, this);
    // Re-render when any bound value changes.
    lv_subject_add_observer_obj(view_model().radio_state_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().region_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().owner_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().modem_preset_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().hop_limit_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().frequency_slot_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().position_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().config_state_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().mqtt_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().autostart_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().channels_version_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().messages_version_subject(), on_change, list_, this);
    lv_subject_add_observer_obj(view_model().dark_mode_subject(), on_change, list_, this);
}

void SettingsScreen::on_change(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<SettingsScreen*>(lv_observer_get_user_data(observer));
    if (self && self->overlay_mode_ == Overlay::None) {
        self->rebuild();
    }
}

std::string SettingsScreen::value_for(int row) {
    if (row == 2 || row == 3 || row == 6 || row == 7 || row == 17) {
        const std::string state = view_model().config_state();
        if (state != "confirmed") return state == "pending" ? "Pending..." : "Unknown";
    }
    switch (row) {
        case 0: return s_of(view_model().radio_state_subject()) + " " + view_model().radio_mode();
        case 18: return view_model().setup().str_at("mode");
        case 19: return view_model().radio_mode();
        case 23: return view_model().history_enabled() ? "On" : "Off";
        case 24: return view_model().quiet_start() < 0 ? "Off" : "Scheduled";
        case 1: return s_of(view_model().region_subject());
        case 2: return s_of(view_model().modem_preset_subject());
        case 3: return std::to_string(lv_subject_get_int(view_model().hop_limit_subject()));
        case 4: return "";
        case 5: return s_of(view_model().owner_subject());
        case 6:
            if (!lv_subject_get_int(view_model().position_subject())) return "Off";
            for (const auto& ch : view_model().channels()) if (ch.positionPrecision > 0)
                return std::string(ch.isPrivate ? "Private: " : "Public: ") + std::to_string(ch.index);
            return "On";
        case 7: return lv_subject_get_int(view_model().mqtt_subject()) ? "On" : "Off";
        case 8: return lv_subject_get_int(view_model().autostart_subject()) ? "On" : "Off";
        case 9: return view_model().is_dark_mode() ? "Dark" : "Light";
        case 10: return std::to_string(view_model().channels().size());
        case 11: return "";
        case 12: return std::to_string(view_model().canned_messages().size());
        case 13: return view_model().show_signal() ? "On" : "Off";
        case 14: return view_model().retention_cap() == 0 ? "All"
                                                          : std::to_string(view_model().retention_cap());
        case 15: return view_model().mute_all() ? "On" : "Off";
        case 16: return view_model().want_ack() ? "On" : "Off";
        case 17: {
            const int slot = lv_subject_get_int(view_model().frequency_slot_subject());
            return slot == 0 ? "Auto" : std::to_string(slot);
        }
        default: return "";
    }
}

void SettingsScreen::rebuild() {
    lv_obj_clean(list_);
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* font = assets().load_standard_font(14, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    lv_obj_t* sel_row = nullptr;
    for (int i = 0; i < kRowCount; ++i) {
        auto* row = lv_obj_create(list_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        if (i == selected_) {
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(row, colors.button, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(row, 3, 0);
            lv_obj_set_style_border_color(row, colors.primary, 0);
            sel_row = row;
        }
        auto* label = lv_label_create(row);
        lv_label_set_text(label, kRows[i]);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, colors.text, 0);

        const std::string value = value_for(i);
        auto* val = lv_label_create(row);
        lv_label_set_text(val, value.c_str());
        lv_obj_set_style_text_font(val, font, 0);
        lv_obj_set_style_text_color(val, colors.text_disabled, 0);
    }
    lv_obj_update_layout(list_);
    if (sel_row) lv_obj_scroll_to_view(sel_row, LV_ANIM_OFF);
}

void SettingsScreen::row_action(int row) {
    if (row == 18) { request_service("setup", "setup.status"); return; }
    if (row == 19) { open_picker("Antenna attached? Region: " + (view_model().radio_mode() == "off" ? view_model().setup().str_at("region", "Unknown") : s_of(view_model().region_subject())), {"Cancel", "Enable transmission", "Receive only"}, "tx", 0); return; }
    if (row == 20) { request_service("slots", "channel.slots"); return; }
    if (row == 21) { request_service("profiles", "profile.list"); return; }
    if (row == 22) { open_picker("Diagnostics", {"View current health", "Export sanitized health"}, "diagnostics", 0); return; }
    if (row == 23) { open_picker("Disk history (private, not encrypted)", {"Cancel", view_model().history_enabled() ? "Erase saved history; RAM only" : "Save current and future chats"}, "history", 0); return; }
    if (row == 24) { open_picker("Quiet hours (device local time)", {"Off", "22:00-07:00", "Custom times"}, "quiet", 0); return; }
    if (row == 17) {
        if (std::string(view_model().config_state()) != "confirmed") { view_model().apply_notice("Wait for confirmed radio settings"); return; }
        open_text("Frequency slot (0=Auto)", std::to_string(lv_subject_get_int(view_model().frequency_slot_subject())), "frequency-slot");
        return;
    }
    switch (row) {
        case 0: {  // Radio toggle
            if (s_of(view_model().radio_state_subject()) == "online") view_model().radio_stop();
            else view_model().radio_start();
            break;
        }
        case 1: open_picker("Region", kRegions, "region", index_of(kRegions, s_of(view_model().region_subject()))); break;
        case 2: open_picker("Modem preset", kPresets, "preset", index_of(kPresets, s_of(view_model().modem_preset_subject()))); break;
        case 3: {
            std::vector<std::string> hops;
            for (int h = 1; h <= 7; ++h) hops.push_back(std::to_string(h));
            open_picker("Hop limit", hops, "hop", lv_subject_get_int(view_model().hop_limit_subject()) - 1);
            break;
        }
        case 4: open_picker("Add a channel", {"Scan QR code", "Paste share URL"}, "addmenu", 0); break;
        case 5: open_text("Node name", s_of(view_model().owner_subject()), "name"); break;
        case 6: {
            if (std::string(view_model().config_state()) != "confirmed") {
                view_model().apply_notice("Wait for confirmed radio settings");
                break;
            }
            if (lv_subject_get_int(view_model().position_subject())) {
                view_model().set_position_sharing(false);
            } else {
                position_channels_.clear();
                position_channel_ids_.clear();
                std::vector<std::string> names;
                for (const auto& ch : view_model().channels()) {
                    position_channels_.push_back(ch.index);
                    position_channel_ids_.push_back(ch.identity);
                    names.push_back(view_model().channel_label(ch.index));
                }
                if (names.empty()) view_model().apply_notice("Configure a channel first");
                else open_picker("Share position on", names, "position-channel", 0);
            }
            break;
        }
        case 7: view_model().set_mqtt(!lv_subject_get_int(view_model().mqtt_subject())); break;
        case 8: view_model().set_auto_start(!lv_subject_get_int(view_model().autostart_subject())); break;
        case 9: view_model().toggle_dark_mode(); break;
        case 10: {
            std::vector<std::string> rows;
            for (const auto& c : view_model().channels()) {
                rows.push_back(view_model().channel_label(c.index) + " " + c.role +
                    (c.positionPrecision ? " / Location On" : " / Location Off"));
            }
            if (rows.empty()) rows.push_back("No channels yet.");
            open_info("Channels", rows);
            break;
        }
        case 11: {
            open_info("About", {
                std::string("Version:  ") + app::kAppVersion,
                std::string("Source:  ") + app::kRepoUrl,
                "Node ID:  " + s_of(view_model().node_id_subject()),
                "Firmware:  " + s_of(view_model().firmware_subject()),
                "Model:  " + s_of(view_model().model_subject()),
                "Region:  " + s_of(view_model().region_subject()),
                "Name:  " + s_of(view_model().owner_subject()),
            });
            break;
        }
        case 12: open_canned(); break;
        case 13: view_model().set_show_signal(!view_model().show_signal()); rebuild(); break;
        case 14: {
            const int cap = view_model().retention_cap();
            int initial = 0;
            if (cap == 50) initial = 1; else if (cap == 100) initial = 2; else if (cap == 200) initial = 3;
            open_picker("Keep per chat", {"All", "50", "100", "200"}, "retention", initial);
            break;
        }
        case 15: view_model().set_mute_all(!view_model().mute_all()); rebuild(); break;
        case 16: view_model().set_want_ack(!view_model().want_ack()); rebuild(); break;
        default: break;
    }
}

// --- overlays ---------------------------------------------------------------
void SettingsScreen::open_picker(const std::string& title, std::vector<std::string> options,
                                 const std::string& pending, int initial) {
    overlay_mode_ = Overlay::Picker;
    overlay_title_ = title;
    options_ = std::move(options);
    pending_ = pending;
    opt_sel_ = (initial < 0) ? 0 : initial;
    if (opt_sel_ >= static_cast<int>(options_.size())) opt_sel_ = 0;
    overlay_obj_ = lv_obj_create(lv_obj_get_parent(list_));
    lv_obj_remove_style_all(overlay_obj_);
    lv_obj_set_size(overlay_obj_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_obj_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_obj_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(overlay_obj_, 6, 0);
    lv_obj_set_style_pad_row(overlay_obj_, 2, 0);
    lv_obj_clear_flag(overlay_obj_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_picker();
}

void SettingsScreen::rebuild_picker() {
    lv_obj_clean(overlay_obj_);
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    auto* head = lv_label_create(overlay_obj_);
    lv_obj_set_width(head, lv_pct(100));
    lv_label_set_long_mode(head, LV_LABEL_LONG_WRAP);
    lv_label_set_text(head, overlay_title_.c_str());
    lv_obj_set_style_text_font(head, font, 0);
    lv_obj_set_style_text_color(head, colors.text, 0);

    if (opt_sel_ < 0) opt_sel_ = 0;
    if (opt_sel_ >= static_cast<int>(options_.size())) opt_sel_ = static_cast<int>(options_.size()) - 1;

    lv_obj_t* sel = nullptr;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        auto* row = lv_label_create(overlay_obj_);
        lv_label_set_text(row, options_[i].c_str());
        lv_obj_set_style_text_font(row, font, 0);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_pad_ver(row, 3, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        if (i == opt_sel_) {
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(row, colors.button, 0);
            lv_obj_set_style_text_color(row, colors.primary, 0);
            sel = row;
        } else {
            lv_obj_set_style_text_color(row, colors.text, 0);
        }
    }
    lv_obj_update_layout(overlay_obj_);
    if (sel) lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
}

void SettingsScreen::open_text(const std::string& title, const std::string& initial,
                               const std::string& pending) {
    overlay_mode_ = Overlay::Text;
    overlay_title_ = title;
    pending_ = pending;
    overlay_obj_ = lv_obj_create(lv_obj_get_parent(list_));
    lv_obj_remove_style_all(overlay_obj_);
    lv_obj_set_size(overlay_obj_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_obj_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_obj_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(overlay_obj_, 8, 0);
    lv_obj_set_style_pad_row(overlay_obj_, 6, 0);
    lv_obj_clear_flag(overlay_obj_, LV_OBJ_FLAG_SCROLLABLE);
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    auto* head = lv_label_create(overlay_obj_);
    lv_obj_set_width(head, lv_pct(100));
    lv_label_set_long_mode(head, LV_LABEL_LONG_WRAP);
    lv_label_set_text(head, title.c_str());
    lv_obj_set_style_text_font(head, font, 0);
    lv_obj_set_style_text_color(head, colors.text, 0);

    input_ = lv_textarea_create(overlay_obj_);
    lv_textarea_set_one_line(input_, true);
    lv_textarea_set_text(input_, initial.c_str());
    lv_obj_set_width(input_, lv_pct(100));
    lv_obj_set_style_text_font(input_, font, 0);
    lv_group_t* g = lv_group_get_default();
    if (g) { lv_group_add_obj(g, input_); lv_group_focus_obj(input_); }

    auto* hint = lv_label_create(overlay_obj_);
    lv_label_set_text(hint, "Enter: save   \xC2\xB7   Esc: cancel");
    lv_obj_set_style_text_font(hint, font, 0);
    lv_obj_set_style_text_color(hint, colors.text_disabled, 0);
}

void SettingsScreen::open_info(const std::string& title, std::vector<std::string> rows) {
    overlay_mode_ = Overlay::Info;
    overlay_title_ = title;
    overlay_obj_ = lv_obj_create(lv_obj_get_parent(list_));
    lv_obj_remove_style_all(overlay_obj_);
    lv_obj_set_size(overlay_obj_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_obj_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_obj_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(overlay_obj_, 8, 0);
    lv_obj_set_style_pad_row(overlay_obj_, 3, 0);
    lv_obj_clear_flag(overlay_obj_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    const lv_font_t* title_font = assets().load_standard_font(15);
    if (!title_font) title_font = &lv_font_montserrat_16;
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    auto* head = lv_label_create(overlay_obj_);
    lv_obj_set_width(head, lv_pct(100));
    lv_label_set_long_mode(head, LV_LABEL_LONG_WRAP);
    lv_label_set_text(head, title.c_str());
    lv_obj_set_style_text_font(head, title_font, 0);
    lv_obj_set_style_text_color(head, colors.primary, 0);
    for (const auto& r : rows) {
        auto* row = lv_label_create(overlay_obj_);
        lv_label_set_text(row, r.c_str());
        lv_label_set_long_mode(row, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_text_font(row, font, 0);
        lv_obj_set_style_text_color(row, colors.text, 0);
    }
    auto* hint = lv_label_create(overlay_obj_);
    lv_label_set_text(hint, "Esc: back");
    lv_obj_set_style_text_font(hint, font, 0);
    lv_obj_set_style_text_color(hint, colors.text_disabled, 0);
}

void SettingsScreen::open_canned() {
    overlay_mode_ = Overlay::Canned;
    overlay_title_ = "Quick messages";
    pending_.clear();
    options_.clear();
    options_.push_back("+ Add new");
    for (const auto& c : view_model().canned_messages()) options_.push_back(c);
    opt_sel_ = 0;
    overlay_obj_ = lv_obj_create(lv_obj_get_parent(list_));
    lv_obj_remove_style_all(overlay_obj_);
    lv_obj_set_size(overlay_obj_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_obj_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_obj_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_obj_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(overlay_obj_, 6, 0);
    lv_obj_set_style_pad_row(overlay_obj_, 2, 0);
    lv_obj_clear_flag(overlay_obj_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_picker();
}

void SettingsScreen::teardown_overlay_obj() {
    if (input_) {
        lv_group_t* g = lv_group_get_default();
        if (g) lv_group_remove_obj(input_);
        input_ = nullptr;
    }
    if (overlay_obj_) { lv_obj_delete(overlay_obj_); overlay_obj_ = nullptr; }
}

void SettingsScreen::close_overlay() {
    if (input_) {
        lv_group_t* g = lv_group_get_default();
        if (g) lv_group_remove_obj(input_);
        input_ = nullptr;
    }
    if (overlay_obj_) { lv_obj_delete(overlay_obj_); overlay_obj_ = nullptr; }
    overlay_mode_ = Overlay::None;
    options_.clear();
    pending_.clear();
    from_canned_ = false;
    canned_edit_index_ = -1;
    rebuild();
}

void SettingsScreen::apply_overlay() {
    if (overlay_mode_ == Overlay::Picker) {
        if (opt_sel_ < 0 || opt_sel_ >= static_cast<int>(options_.size())) { close_overlay(); return; }
        const std::string choice = options_[opt_sel_];
        if (extended_picker(pending_, opt_sel_)) return;
        if (pending_ == "region") { view_model().set_region(choice); close_overlay(); }
        else if (pending_ == "preset") { view_model().set_modem_preset(choice); close_overlay(); }
        else if (pending_ == "hop") { view_model().set_hop_limit(opt_sel_ + 1); close_overlay(); }
        else if (pending_ == "position-channel") {
            const int index = opt_sel_ >= 0 && opt_sel_ < static_cast<int>(position_channels_.size()) ? position_channels_[opt_sel_] : -1;
            const auto* ch = view_model().channel(index);
            const bool same = ch && opt_sel_ >= 0 && opt_sel_ < static_cast<int>(position_channel_ids_.size()) && ch->identity == position_channel_ids_[opt_sel_];
            const bool is_public = ch && !ch->isPrivate;
            const bool valid = same;
            const std::string identity = ch ? ch->identity : "";
            close_overlay();
            if (!valid) view_model().apply_notice("Channel changed; choose again before sharing location");
            else if (is_public) {
                public_position_channel_ = index;
                public_position_identity_ = identity;
                open_picker("Public location (~730 m)?", {"Cancel", "Enable: " + view_model().channel_label(index)}, "position-public", 0);
            } else if (valid) view_model().set_position_sharing(true, index);
        }
        else if (pending_ == "position-public") {
            const bool confirmed = opt_sel_ == 1;
            const int index = public_position_channel_;
            const auto* channel = view_model().channel(index);
            const bool same = channel && channel->identity == public_position_identity_;
            close_overlay();
            public_position_channel_ = -1;
            if (confirmed && same) view_model().set_position_sharing(true, index, true);
            else if (confirmed) view_model().apply_notice("Channel changed; public location was not enabled");
        }
        else if (pending_ == "retention") {
            view_model().set_retention_cap(choice == "All" ? 0 : std::atoi(choice.c_str()));
            close_overlay();
        }
        else if (pending_ == "addmenu") {
            if (opt_sel_ == 0) {
                view_model().scan_channel_qr();
                const bool have_overlay = overlay_obj_ != nullptr;
                (void)have_overlay;
                close_overlay();
                open_info("Scan QR code", {"Point the camera at a Meshtastic channel QR code.",
                                           "(Works on the device, not the simulator.)"});
            } else {
                close_overlay();
                open_text("Paste share URL", "", "channel-url");
            }
        } else {
            close_overlay();
        }
    } else if (overlay_mode_ == Overlay::Canned) {
        const int choice = opt_sel_;
        teardown_overlay_obj();
        if (choice == 0) {
            from_canned_ = true; canned_edit_index_ = -1;
            open_text("New quick message", "", "canned-add");
        } else {
            const int idx = choice - 1;
            const auto& canned = view_model().canned_messages();
            const std::string cur = (idx >= 0 && idx < static_cast<int>(canned.size())) ? canned[idx] : "";
            from_canned_ = true; canned_edit_index_ = idx;
            open_text("Edit (empty = delete)", cur, "canned-edit");
        }
    } else if (overlay_mode_ == Overlay::Text) {
        const char* text = input_ ? lv_textarea_get_text(input_) : "";
        const std::string value = text ? text : "";
        const std::string pending = pending_;
        if (extended_text(pending, value)) return;
        if (pending == "name" && !value.empty()) {
            view_model().set_owner(value, value.substr(0, 4));
        } else if (pending == "channel-url" && !value.empty()) {
            view_model().add_channel_url(value);
        } else if (pending == "frequency-slot") {
            char* end = nullptr;
            errno = 0;
            const long slot = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || errno || !end || *end || slot < 0 || slot > 255)
                view_model().apply_notice("Enter a frequency slot from 0 to 255 (0=Auto)");
            else view_model().set_frequency_slot(static_cast<int>(slot));
        } else if (pending == "canned-add") {
            view_model().add_canned(value);
        } else if (pending == "canned-edit") {
            view_model().update_canned(canned_edit_index_, value);  // empty => delete
        }
        if (from_canned_) {
            from_canned_ = false;
            teardown_overlay_obj();
            open_canned();
        } else {
            close_overlay();
        }
    } else {
        close_overlay();
    }
}

// --- navigation -------------------------------------------------------------
void SettingsScreen::on_up() {
    if (overlay_mode_ == Overlay::Picker || overlay_mode_ == Overlay::Canned) {
        if (opt_sel_ > 0) { --opt_sel_; rebuild_picker(); }
        return;
    }
    if (overlay_mode_ != Overlay::None) return;
    if (selected_ > 0) { --selected_; rebuild(); }
}

void SettingsScreen::on_down() {
    if (overlay_mode_ == Overlay::Picker || overlay_mode_ == Overlay::Canned) {
        if (opt_sel_ < static_cast<int>(options_.size()) - 1) { ++opt_sel_; rebuild_picker(); }
        return;
    }
    if (overlay_mode_ != Overlay::None) return;
    if (selected_ < kRowCount - 1) { ++selected_; rebuild(); }
}

void SettingsScreen::on_enter() {
    if (overlay_mode_ != Overlay::None) { apply_overlay(); return; }
    row_action(selected_);
}

bool SettingsScreen::on_back() {
    // Esc inside a quick-message editor returns to the list, not all the way out.
    if (overlay_mode_ == Overlay::Text && from_canned_) {
        from_canned_ = false;
        teardown_overlay_obj();
        open_canned();
        return true;
    }
    if (overlay_mode_ != Overlay::None) { service_route_.clear(); close_overlay(); return true; }
    return false;
}

bool SettingsScreen::is_composing() {
    return overlay_mode_ == Overlay::Text;
}


namespace {
std::string qstr(const std::string& s) { return "\"" + json::escape(s) + "\""; }
bool flag(const json::Value& value, const std::string& name) { const auto* v = value.find(name); return v && v->as_bool(); }
}
void SettingsScreen::services_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<SettingsScreen*>(lv_observer_get_user_data(observer));
    if (self) self->service_result();
}
void SettingsScreen::request_service(const std::string& route, const std::string& intent, const std::string& fields) {
    close_overlay();
    service_route_ = route;
    open_info("Working", {"Waiting for supervisor...", "Esc cancels this view."});
    if (!view_model().service_request(intent, fields)) { service_route_.clear(); close_overlay(); }
}
void SettingsScreen::service_result() {
    if (service_route_.empty()) {
        if (!offered_setup_ && overlay_mode_ == Overlay::None && view_model().setup().is_obj() && !flag(view_model().setup(), "ready")) {
            offered_setup_ = true;
            open_picker("First run: select your radio region", kRegions, "setup-region", 0);
        }
        return;
    }
    const auto event = view_model().service_event();
    const auto route = service_route_;
    service_route_.clear(); close_overlay();
    if (event.str_at("event") == "error") { open_info("Action failed", {event.str_at("text")}); return; }
    if (route == "setup") {
        if (!flag(event, "ready")) { offered_setup_ = true; open_picker("First run: select your radio region", kRegions, "setup-region", 0); }
        else open_info("Setup ready", {"Region: " + event.str_at("region"), "Mode: " + event.str_at("mode"), "Firewall: " + std::string(flag(event,"firewall") ? "healthy" : "unavailable; radio stays off"), "Use Transmit mode to explicitly enable TX.", "Public channels expose identity and telemetry. Location and MQTT default Off."});
    } else if (route == "slots") {
        service_items_.clear(); std::vector<std::string> rows = {"+ New private channel", "+ New public channel"};
        if (const auto* a = event.find("channels")) for (const auto& c : a->arr) {
            service_items_.push_back(c);
            rows.push_back(std::to_string(c.int_at("index")) + ": " + c.str_at("name", "Default") + " " + c.str_at("role") + (flag(c,"private") ? " Private" : " Public"));
        }
        open_picker("Channel manager", rows, "slots", 0);
    } else if (route == "profiles") {
        service_items_.clear(); std::vector<std::string> rows = {"+ Save current as profile"};
        if (const auto* a = event.find("profiles")) for (const auto& profile : a->arr) { service_items_.push_back(profile); rows.push_back(profile.str_at("name")); }
        open_picker("Local mesh profiles (keys saved privately)", rows, "profiles", 0);
    } else if (route == "preview") {
        std::string title = event.str_at("name") + " / " + event.str_at("region") + " / " + event.str_at("preset") + " / slot " + std::to_string(event.int_at("slot"));
        std::vector<std::string> rows = {"Cancel", "Apply: radio Off, TX/location/MQTT Off", "Delete saved profile"};
        if (const auto* a = event.find("channels")) for (const auto& c : a->arr) rows.push_back(std::to_string(c.int_at("index")) + ": " + c.str_at("name") + (flag(c,"private") ? " [Private]" : " [Public]"));
        open_picker(title, rows, "profile-action", 0);
    } else if (route == "diagnostics") {
        std::vector<std::string> rows = {"App: " + event.str_at("app"), "Daemon: " + event.str_at("daemon"), "Client: " + event.str_at("client"), "Radio: " + event.str_at("radio") + " / " + event.str_at("mode"),
            "API: " + std::string(flag(event,"api_connected") ? "connected" : "disconnected"), "Camera: " + std::string(flag(event,"camera_available") ? "available" : "unavailable"),
            "GPS age: " + std::to_string(event.int_at("gps_age", -1)) + "s (-1=unknown)", "GPS satellites: " + std::to_string(event.int_at("gps_satellites", -1)), "Last RX age: " + std::to_string(event.int_at("last_rx_age", -1)) + "s",
            "History: " + std::to_string(event.int_at("history_count")), view_model().radio_detail()};
        const auto* fw = event.find("firewall"); rows.push_back(std::string("Firewall: ") + (fw && flag(*fw,"ok") ? "healthy" : "FAILED"));
        open_info("Diagnostics", rows);
    } else { open_info("Complete", {"Saved and confirmed.", "Profile/setup changes leave the radio Off in receive-only mode."}); }
    rebuild();
}
bool SettingsScreen::extended_picker(const std::string& input, int choice) {
    const std::string pending = input;
    if (pending == "setup-region") {
        setup_region_ = options_[choice]; close_overlay();
        open_picker("Channel (public identity/telemetry is visible)", {"Private: generate a new secret", "Public default"}, "setup-kind", 0);
    } else if (pending == "setup-kind") {
        setup_kind_ = choice == 0 ? "private" : "public"; close_overlay();
        if (setup_kind_ == "private") open_text("Private channel name (1..11 bytes)", "", "setup-name");
        else { channel_name_.clear(); open_picker(setup_region_ + " / Public default (RX only)", {"Cancel", "Save setup (no transmission)"}, "setup-confirm", 0); }
    } else if (pending == "setup-confirm") {
        close_overlay(); if (choice == 1) request_service("setup", "setup.initialize", ",\"confirmed\":true,\"region\":" + qstr(setup_region_) + ",\"kind\":" + qstr(setup_kind_) + ",\"name\":" + qstr(channel_name_));
    } else if (pending == "tx") {
        close_overlay(); if (choice) request_service("setup", "radio.tx", std::string(",\"antenna_confirmed\":true,\"enabled\":") + (choice == 1 ? "true" : "false"));
    } else if (pending == "slots") {
        if (choice < 2) { setup_kind_ = choice == 0 ? "private" : "public"; close_overlay(); open_text("New channel name (max 11 bytes)", "", "channel-create"); }
        else if (choice - 2 < static_cast<int>(service_items_.size())) {
            const auto c = service_items_[choice-2]; managed_channel_ = c.int_at("index"); channel_id_ = c.str_at("identity");
            channel_fields_ = ",\"channel\":" + std::to_string(managed_channel_) + ",\"channel_id\":" + qstr(channel_id_);
            close_overlay(); open_picker("Edit slot " + std::to_string(managed_channel_), {"Cancel", "Rename", "Rotate private key", "Disable", "Remove", "Enable"}, "channel-action", 0);
        }
    } else if (pending == "channel-action") {
        close_overlay(); if (!choice) return true;
        const std::vector<std::string> actions = {"", "rename", "rotate", "disable", "remove", "enable"}; channel_action_ = actions[choice];
        if (choice == 1) open_text("New channel name (max 11 bytes)", "", "channel-rename");
        else open_picker("Change channel? History keeps original identity.", {"Cancel", "Confirm " + channel_action_}, "channel-confirm", 0);
    } else if (pending == "channel-confirm") {
        close_overlay(); if (choice) request_service("slots", "channel.manage", channel_fields_ + ",\"confirmed\":true,\"action\":" + qstr(channel_action_));
    } else if (pending == "profiles") {
        if (!choice) { close_overlay(); open_text("Save profile name (max 48 bytes)", "", "profile-save"); }
        else if (choice-1 < static_cast<int>(service_items_.size())) { profile_id_ = service_items_[choice-1].str_at("id"); request_service("preview", "profile.preview", ",\"profile\":" + qstr(profile_id_)); }
    } else if (pending == "profile-action") {
        close_overlay(); if (choice == 1 || choice == 2) request_service(choice == 1 ? "setup" : "profiles", choice == 1 ? "profile.apply" : "profile.delete", ",\"confirmed\":true,\"profile\":" + qstr(profile_id_));
    } else if (pending == "diagnostics") {
        request_service("diagnostics", choice ? "diagnostics.export" : "diagnostics.get");
    } else if (pending == "history") {
        close_overlay(); if (choice) { view_model().service_request("history.set", std::string(",\"enabled\":") + (view_model().history_enabled() ? "false" : "true")); }
    } else if (pending == "quiet") {
        close_overlay(); if (!choice) view_model().set_quiet_hours(-1,-1); else if (choice == 1) view_model().set_quiet_hours(22*60,7*60); else open_text("Local HH:MM-HH:MM (e.g. 22:00-07:00)", "", "quiet-times");
    } else return false;
    return true;
}
bool SettingsScreen::extended_text(const std::string& pending, const std::string& value) {
    if (pending == "setup-name") {
        channel_name_ = value; close_overlay(); open_picker(setup_region_ + " / Private: " + channel_name_ + " (RX only)", {"Cancel", "Save setup (no transmission)"}, "setup-confirm", 0);
    } else if (pending == "channel-create") {
        channel_action_ = "create"; channel_fields_ = ",\"kind\":" + qstr(setup_kind_) + ",\"name\":" + qstr(value);
        close_overlay(); open_picker("Create new group? Location/MQTT Off.", {"Cancel", "Create " + setup_kind_}, "channel-confirm", 0);
    } else if (pending == "channel-rename") {
        channel_fields_ += ",\"name\":" + qstr(value); close_overlay(); open_picker("Rename changes the channel identity.", {"Cancel", "Confirm rename"}, "channel-confirm", 0);
    } else if (pending == "profile-save") {
        request_service("profiles", "profile.save", ",\"name\":" + qstr(value));
    } else if (pending == "quiet-times") {
        int sh,sm,eh,em; char extra;
        if (std::sscanf(value.c_str(), "%2d:%2d-%2d:%2d%c", &sh,&sm,&eh,&em,&extra) != 4 || sh<0 || sh>23 || eh<0 || eh>23 || sm<0 || sm>59 || em<0 || em>59 || value.size()!=11) view_model().apply_notice("Use HH:MM-HH:MM in device local time");
        else view_model().set_quiet_hours(sh*60+sm, eh*60+em);
        close_overlay();
    } else return false;
    return true;
}

} // namespace screen
