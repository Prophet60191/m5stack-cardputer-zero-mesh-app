/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "messages_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "list_selection.h"
#include "text.h"

#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <ctime>
#include <string>

namespace screen {
namespace {

constexpr int kBarHeight = 22;
constexpr int kContextHeight = 20;
constexpr int kThreadPageSize = 50;
constexpr const char* kDot = "   \xC2\xB7   ";

void fmt(const char* pattern, char* buf, std::size_t n, long t) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    std::strftime(buf, n, pattern, &lt);
}

bool same_day(long a, long b) {
    char da[12], db[12];
    fmt("%Y%m%d", da, sizeof(da), a);
    fmt("%Y%m%d", db, sizeof(db), b);
    return std::string(da) == std::string(db);
}

std::string thread_of(const viewmodel::MessageInfo& m) {
    return viewmodel::thread_key_of(m);   // one canonical definition
}

// Action-menu ids.
enum {
    A_MARK_READ = 1, A_MUTE, A_CLEAR, A_DELETE_THREAD, A_INFO,
    A_MSG_DETAILS, A_MSG_QUOTE, A_MSG_RESEND, A_MSG_DELETE,
    A_SEND_LOCATION, A_REQUEST_POSITION, A_TRACEROUTE, A_SHARE_CHANNEL, A_CHANNEL, A_PIN,
};

std::string status_text(int s) {
    switch (s) {
        case viewmodel::kMsgSending: return "sending\xE2\x80\xA6";  // sending…
        case viewmodel::kMsgDelivered: return "Recipient received";  // ✓ delivered
        case viewmodel::kMsgMesh: return "Mesh confirmed";
        case viewmodel::kMsgSent: return "Receipt unconfirmed";
        case viewmodel::kMsgFailed: return "\xE2\x9C\x97 failed";    // ✗ failed
        default: return "";
    }
}

std::string preview(const std::string& s) {
    return view::preview_text(s, 32);
}

}  // namespace

MessagesScreen::MessagesScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void MessagesScreen::build_content(lv_obj_t* content) {
    body_ = lv_obj_create(content);
    lv_obj_remove_style_all(body_);
    lv_obj_set_size(body_, lv_pct(100), view::kScreenHeight - view::kTitleBarHeight - kBarHeight);
    lv_obj_align(body_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body_, 3, 0);
    lv_obj_set_style_pad_all(body_, 4, 0);
    lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    view::widgets::style_list_scrollbar(body_, view::palette(view_model().is_dark_mode()));

    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_12;

    context_ = lv_label_create(content);
    const lv_font_t* context_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(context_, context_font ? context_font : font, 0);
    lv_obj_set_style_text_color(context_, colors.primary, 0);
    lv_obj_set_size(context_, view::kScreenWidth - 8, kContextHeight);
    lv_obj_set_style_pad_top(context_, 3, 0);
    lv_label_set_long_mode(context_, LV_LABEL_LONG_DOT);
    lv_obj_align(context_, LV_ALIGN_TOP_LEFT, 4, 0);
    lv_obj_add_flag(context_, LV_OBJ_FLAG_HIDDEN);

    tabs_ = lv_obj_create(content);
    lv_obj_remove_style_all(tabs_);
    lv_obj_set_size(tabs_, lv_pct(100), kContextHeight);
    lv_obj_clear_flag(tabs_, LV_OBJ_FLAG_SCROLLABLE);
    channel_tab_ = lv_label_create(tabs_);
    direct_tab_ = lv_label_create(tabs_);
    for (auto* label : {channel_tab_, direct_tab_}) {
        lv_obj_set_style_text_font(label, context_font ? context_font : font, 0);
        lv_obj_set_size(label, 152, kContextHeight);
        lv_obj_set_style_pad_top(label, 2, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    }
    lv_label_set_text(channel_tab_, "Channels");
    lv_label_set_text(direct_tab_, "Direct Messages");
    lv_obj_align(channel_tab_, LV_ALIGN_TOP_LEFT, 4, 0);
    lv_obj_align(direct_tab_, LV_ALIGN_TOP_RIGHT, -4, 0);

    bar_ = lv_obj_create(content);
    lv_obj_remove_style_all(bar_);
    lv_obj_set_size(bar_, lv_pct(100), kBarHeight);
    lv_obj_align(bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

    hint_ = lv_label_create(bar_);
    lv_obj_set_style_text_font(hint_, font, 0);
    lv_obj_set_style_text_color(hint_, colors.text, 0);
    lv_obj_set_width(hint_, view::kScreenWidth - 8);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_CLIP);
    lv_obj_align(hint_, LV_ALIGN_LEFT_MID, 4, 0);

    input_ = lv_textarea_create(bar_);

    lv_obj_set_style_bg_color(input_, colors.surface, 0);
    lv_obj_set_style_bg_opa(input_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(input_, colors.text, 0);
    lv_obj_set_style_text_color(input_, colors.text_disabled, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(input_, colors.border, 0);
    // Keep the 22px typing bar, with room for the device font's 17px line.
    lv_obj_set_style_border_width(input_, 1, 0);
    lv_obj_set_style_bg_color(input_, colors.text, LV_PART_CURSOR);
    lv_obj_set_style_border_color(input_, colors.text, LV_PART_CURSOR);
    lv_obj_add_event_cb(input_, draft_changed, LV_EVENT_VALUE_CHANGED, this);
    lv_textarea_set_max_length(input_, 200);
    lv_textarea_set_one_line(input_, true);
    lv_textarea_set_placeholder_text(input_, "Type a message");
    lv_obj_set_size(input_, lv_pct(100), kBarHeight);
    lv_obj_set_style_pad_ver(input_, 1, 0);
    lv_obj_set_style_text_font(input_, font, 0);
    lv_obj_add_flag(input_, LV_OBJ_FLAG_HIDDEN);

    // If we arrived here to DM someone, open straight into that thread.
    const std::string group = view_model().take_group_target();
    const std::string& dm = view_model().dm_target_id();
    if (!group.empty()) {
        mode_ = Mode::List;
        for (const auto &ch : view_model().channels())
            if (ch.identity == group) {
                open_thread("ch:" + group, "", "");
                break;
            }
    } else if (!dm.empty()) {
        open_thread("dm:" + dm, dm, view_model().dm_target_name());
    } else {
        mode_ = Mode::List;
    }
    lv_subject_add_observer_obj(view_model().messages_version_subject(), on_change, body_, this);
    lv_subject_add_observer_obj(view_model().channels_version_subject(), on_change, body_, this);
    lv_subject_add_observer_obj(view_model().node_names_subject(), on_nodes, body_, this);
    lv_subject_add_observer_obj(view_model().traceroute_subject(), traceroute_cb, body_, this);
    lv_subject_add_observer_obj(view_model().channel_share_subject(), channel_share_cb, body_, this);
    subscribing_ = false;
    if (dm.empty())
        rebuild();
}

std::string MessagesScreen::peer_name_for(const std::string& id) {
    for (const auto& n : view_model().nodes()) {
        if (n.id == id) return n.name.empty() ? id : n.name;
    }
    return id;
}

std::string MessagesScreen::channel_name(int index) {
    return view_model().channel_label(index);
}

std::string MessagesScreen::group_name(const std::string& key) {
    const int index = view_model().channel_for_thread(key);
    if (index >= 0) return "# " + channel_name(index);
    for (const auto& m : view_model().messages()) if (thread_of(m) == key)
        return "Archived: " + (m.channelName.empty() ? "Default" : m.channelName) + (m.channelPrivate ? " [Private]" : " [Public]");
    return "Archived channel";
}

void MessagesScreen::update_hint() {
    if (!hint_ || !context_) return;
    const bool thread = mode_ == Mode::Thread;
    const bool filtered = !thread && !search_.empty();
    if (thread || filtered) lv_obj_remove_flag(context_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(context_, LV_OBJ_FLAG_HIDDEN);
    if (thread || filtered) lv_obj_add_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
    update_tabs();
    const int top = kContextHeight;
    lv_obj_set_height(body_, view::kScreenHeight - view::kTitleBarHeight - kBarHeight - top);
    lv_obj_align(body_, LV_ALIGN_TOP_MID, 0, top);
    if (mode_ == Mode::List) {
        lv_label_set_text(context_, filtered ? ("Search: " + search_).c_str() : "");
        lv_label_set_text(hint_, filtered ? "Enter: open result   /: Search" : "Tab: switch   /: Search");
        const int total = list_offset() + static_cast<int>(conversations_.size());
        if (total > kThreadPageSize)
            lv_label_set_text(hint_, (std::to_string(selected_ + 1) + "/" + std::to_string(total) + "   /: Search").c_str());
    } else {
        const int active = send_channel();
        const std::string who = viewmodel::is_group_thread(thread_key_) ? group_name(thread_key_)
        : ("DM via " + (active < 0 ? "choose channel" : channel_name(active)) + " to " + peer_name_);
        lv_label_set_text(context_, who.c_str());
        const char* keys = (msg_sel_ >= 0)
            ? "Enter: actions  o: options"
            : "Enter: write  q: quick  o: options";
        lv_label_set_text(hint_, keys);
    }
}

void MessagesScreen::update_tabs() {
    if (!channel_tab_ || !direct_tab_) return;
    const auto colors = view::palette(view_model().is_dark_mode());
    for (auto* label : {channel_tab_, direct_tab_}) {
        const bool active = (label == channel_tab_) == (tab_ == Tab::Channels);
        lv_obj_set_style_text_color(label, active ? colors.primary : colors.text_disabled, 0);
        lv_obj_set_style_border_side(label, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(label, active ? 2 : 0, 0);
        lv_obj_set_style_border_color(label, colors.primary, 0);
    }
}

int MessagesScreen::send_channel() {
    // A group thread carries its own channel index; never fall back to a
    // different channel. DMs have their own explicit selection.
    if (mode_ == Mode::Thread && viewmodel::is_group_thread(thread_key_)) {
        const int idx = view_model().channel_for_thread(thread_key_);
        return view_model().channel(idx) ? idx : -1;
    }
    const auto* current = view_model().channel(dm_channel_);
    return current && current->identity == dm_channel_id_ ? dm_channel_ : -1;
}

void MessagesScreen::on_change(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<MessagesScreen*>(lv_observer_get_user_data(observer));
    if (self && !self->subscribing_) {
        if (self->share_visible_ && !self->view_model().share_channel_valid()) {
            self->close_info();
            self->view_model().apply_notice("Channel changed; reopen its share code");
        }
        self->rebuild();
    }
}

void MessagesScreen::on_nodes(lv_observer_t *observer, lv_subject_t *) {
    auto *self = static_cast<MessagesScreen *>(lv_observer_get_user_data(observer));
    if (!self || self->subscribing_)
        return;
    if (self->picker_)
        self->rebuild_picker();
    else if (self->mode_ == Mode::List)
        self->rebuild_list();
    else {
        if (!self->peer_id_.empty())
            self->peer_name_ = self->peer_name_for(self->peer_id_);
        self->update_hint();
    }
}

void MessagesScreen::rebuild() {
    if (picker_) { rebuild_picker(); return; }
    update_hint();
    if (mode_ == Mode::List) rebuild_list();
    else rebuild_thread();
}

// ---------------------------------------------------------------------------
// Conversations list
// ---------------------------------------------------------------------------
void MessagesScreen::rebuild_list() {
    update_hint();
    const int old_index = selected_ - list_offset();
    const Conversation selected_chat = old_index >= 0 && old_index < static_cast<int>(conversations_.size())
        ? conversations_[old_index] : Conversation{};
    conversation_rows_.clear();
    thread_rows_.clear();
    thread_bubbles_.clear();
    thread_keys_.clear();
    thread_msg_ids_.clear();
    lv_obj_clean(body_);
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* name_font = assets().load_standard_font(14);
    if (!name_font) name_font = &lv_font_montserrat_14;
    const lv_font_t* sub_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    if (!sub_font) sub_font = &lv_font_montserrat_12;

    std::vector<viewmodel::ConversationRow> rows_data;
    conversations_.clear();
    if (!search_.empty()) {
        for (auto& result : view_model().search_messages(search_)) {
            const std::string name = (viewmodel::is_group_thread(result.key) ? "Channel: " : "DM: ") + result.name;
            rows_data.push_back({result.key, result.peer, name, result.text, result.time});
            conversations_.push_back({result.key, result.peer, name, result.message_key});
        }
    } else {
        for (auto& row : view_model().conversation_rows()) {
            if (viewmodel::is_group_thread(row.key) != (tab_ == Tab::Channels)) continue;
            conversations_.push_back({row.key, row.peer, row.name, ""});
            rows_data.push_back(std::move(row));
        }
    }
    if (!selected_chat.key.empty()) {
        const auto selected = std::find_if(conversations_.begin(), conversations_.end(), [&](const auto& row) {
            return row.key == selected_chat.key && row.message_key == selected_chat.message_key;
        });
        if (selected != conversations_.end()) selected_ = static_cast<int>(selected - conversations_.begin()) + list_offset();
    }
    const int rows = list_offset() + static_cast<int>(conversations_.size());
    selected_ = rows == 0 ? 0 : std::clamp(selected_, 0, rows - 1);
    list_start_ = (selected_ / kThreadPageSize) * kThreadPageSize;

    lv_obj_t* sel_row = nullptr;
    const auto make_row = [&](int i) -> lv_obj_t* {
        auto* row = lv_obj_create(body_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(row, 1, 0);
        lv_obj_set_style_pad_ver(row, 3, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        conversation_rows_.push_back(row);
        view::widgets::style_list_row(row, colors, i == selected_);
        if (i == selected_)
            sel_row = row;
        return row;
    };

    // Search is one flat message-result list, without a New message pseudo-result.
    if (search_.empty() && list_start_ == 0) {
        auto* row = make_row(0);
        auto* label = lv_label_create(row);
        lv_label_set_text(label, "+  New message");
        lv_obj_set_style_text_font(label, name_font, 0);
        lv_obj_set_style_text_color(label, colors.primary, 0);
    }

    if (!search_.empty() && conversations_.empty()) {
        auto *empty = lv_label_create(body_);
        lv_label_set_text(empty, "No messages match this search.");
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_text_font(empty, sub_font, 0);
        lv_obj_set_style_text_color(empty, colors.text_disabled, 0);
    }

    for (int i = 0; i < static_cast<int>(conversations_.size()); ++i) {
        const int logical_index = i + list_offset();
        if (logical_index < list_start_ || logical_index >= list_start_ + kThreadPageSize) continue;
        auto* row = make_row(i + list_offset());
        auto* name = lv_label_create(row);
        lv_label_set_text(name, conversations_[i].name.c_str());
        lv_obj_set_width(name, lv_pct(100));
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_height(name, lv_font_get_line_height(name_font));
        lv_obj_set_style_text_font(name, name_font, 0);
        lv_obj_set_style_text_color(name, colors.text, 0);
        auto* sub = lv_label_create(row);
        char when[16];
        when[0] = '\0';
        if (rows_data[i].last_time > 0)
            fmt("%H:%M", when, sizeof(when), rows_data[i].last_time);
        lv_label_set_text(
            sub, (preview(rows_data[i].preview) + (when[0] ? std::string(kDot) + when : "")).c_str());
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub, lv_pct(100));
        lv_obj_set_height(sub, lv_font_get_line_height(sub_font));
        lv_obj_set_style_text_font(sub, sub_font, 0);
        lv_obj_set_style_text_color(sub, colors.text_disabled, 0);
    }

    update_hint();
    lv_obj_update_layout(body_);
    if (sel_row) lv_obj_scroll_to_view(sel_row, LV_ANIM_OFF);
}

// ---------------------------------------------------------------------------
// One thread (bubbles)
// ---------------------------------------------------------------------------
void MessagesScreen::rebuild_thread() {
    const int previous_scroll = lv_obj_get_scroll_y(body_);
    conversation_rows_.clear();
    lv_obj_clean(body_);
    const lv_font_t* text_font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!text_font) text_font = &lv_font_montserrat_14;
    const lv_font_t* small_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    if (!small_font) small_font = text_font;
    const auto colors = view::palette(view_model().is_dark_mode());

    thread_rows_.clear();
    thread_bubbles_.clear();
    thread_keys_.clear();
    thread_msg_ids_.clear();
    if (msg_sel_ < 0)
        selected_message_.clear();
    const std::string selected_key = selected_message_;
    if (!selected_key.empty())
        msg_sel_ = -1;

    // Locate selection by identity, then build only one page of bubbles. Full
    // history stays in the model/search; entering an old result never builds
    // thousands of LVGL objects just to scroll most of them off screen.
    int count = 0;
    int selected_global = -1;
    for (const auto& m : view_model().messages()) {
        if (thread_of(m) == thread_key_) {
            const std::string key = m.id.empty() ? "" : m.from + "\n" + m.id;
            if (!selected_key.empty() && key == selected_key)
                selected_global = count;
            ++count;
        }
    }
    thread_total_ = count;
    const int last_start = std::max(0, count - kThreadPageSize);
    if (thread_start_ < 0 || (at_bottom_ && selected_key.empty())) thread_start_ = last_start;
    thread_start_ = std::clamp(thread_start_, 0, last_start);
    if (selected_global >= 0 && (selected_global < thread_start_ || selected_global >= thread_start_ + kThreadPageSize))
        thread_start_ = std::clamp(selected_global - kThreadPageSize / 2, 0, last_start);
    msg_sel_ = selected_global < 0 ? -1 : selected_global - thread_start_;
    if (msg_sel_ < 0)
        selected_message_.clear();
    update_hint();

    bool any = false;
    long prev_day = 0;
    int midx = -1;
    int global_index = -1;
    if (thread_start_ > 0) {
        auto* older = lv_label_create(body_);
        lv_label_set_text_fmt(older, "%d older messages (Up)", thread_start_);
        lv_obj_set_style_text_font(older, small_font, 0);
        lv_obj_set_style_text_color(older, colors.text_disabled, 0);
    }
    for (const auto& m : view_model().messages()) {
        if (thread_of(m) != thread_key_) continue;
        ++global_index;
        if (global_index < thread_start_ || global_index >= thread_start_ + kThreadPageSize) continue;
        any = true;
        ++midx;
        const bool selected = (midx == msg_sel_);

        // Date separator when the day changes.
        if (m.time > 0 && (prev_day == 0 || !same_day(prev_day, m.time))) {
            prev_day = m.time;
            char day[16];
            fmt("%b %d", day, sizeof(day), m.time);
            auto* sep = lv_label_create(body_);
            lv_label_set_text(sep, day);
            lv_obj_set_style_text_font(sep, small_font, 0);
            lv_obj_set_style_text_color(sep, colors.text_disabled, 0);
            lv_obj_set_width(sep, lv_pct(100));
            lv_obj_set_style_text_align(sep, LV_TEXT_ALIGN_CENTER, 0);
        }

        auto* row = lv_obj_create(body_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, m.outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        thread_rows_.push_back(row);
        thread_msg_ids_.push_back(m.id);

        auto* col = lv_obj_create(row);
        lv_obj_remove_style_all(col);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        lv_obj_set_height(col, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(col, 232, 0);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, m.outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(col, 1, 0);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

        auto* bubble = lv_obj_create(col);
        thread_bubbles_.push_back(bubble);
        thread_keys_.push_back(m.id.empty() ? "" : m.from + "\n" + m.id);
        lv_obj_remove_style_all(bubble);
        lv_obj_set_width(bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(bubble, 228, 0);
        lv_obj_set_style_radius(bubble, 10, 0);
        lv_obj_set_style_pad_all(bubble, 7, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bubble, m.outgoing ? (view_model().is_dark_mode() ? colors.primary : lv_color_hex(0x087c3a)) : colors.button, 0);
        if (selected) {
            lv_obj_set_style_border_width(bubble, 2, 0);
            lv_obj_set_style_border_color(bubble, colors.error, 0);
            lv_obj_set_style_border_side(bubble, LV_BORDER_SIDE_FULL, 0);
            lv_obj_set_style_border_opa(bubble, LV_OPA_COVER, 0);
            // Draw inside the bubble so its tight parent cannot clip the line.
            // Border + padding stays seven pixels, preserving text/layout bounds.
            lv_obj_set_style_pad_all(bubble, 5, 0);
        }
        lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(bubble, 2, 0);
        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

        if (!m.outgoing && viewmodel::is_group_thread(thread_key_)) {
            std::string who = m.fromName.empty() ? m.from : m.fromName;
            auto* name = lv_label_create(bubble);
            lv_label_set_text(name, who.c_str());
            lv_obj_set_style_max_width(name, 212, 0);
            lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(name, small_font, 0);
            lv_obj_set_style_text_color(name, colors.primary, 0);
        }

        auto* text = lv_label_create(bubble);
        lv_label_set_text(text, m.text.c_str());
        lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(text, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(text, 212, 0);
        lv_obj_set_style_text_font(text, text_font, 0);
        lv_obj_set_style_text_color(text, m.outgoing ? (view_model().is_dark_mode() ? colors.background : lv_color_white()) : colors.text, 0);

        // Footer: time (+ delivery status for our own messages).
        std::string footer;
        if (m.time > 0) { char s[24]; fmt("%H:%M", s, sizeof(s), m.time); footer = s; }
        if (m.outgoing && m.status != 0) {
            if (!footer.empty()) footer += "  ";
            footer += status_text(m.status);
            if (m.status == viewmodel::kMsgDelivered && m.hops >= 0) {
                footer += (m.hops == 0) ? " \xC2\xB7 direct"
                                        : (" \xC2\xB7 " + std::to_string(m.hops) + "h");
            }
        }
        // Inline signal provenance for received messages (when enabled).
        if (!m.outgoing && view_model().show_signal()) {
            std::string sig;
    if (m.hasSnr) { char b[16]; std::snprintf(b, sizeof(b), "%.1f dB", m.snr); sig = b; }
            if (m.hops >= 0) { if (!sig.empty()) sig += " \xC2\xB7 "; sig += std::to_string(m.hops) + "h"; }
            if (!sig.empty()) { if (!footer.empty()) footer += "  "; footer += sig; }
        }
        if (!footer.empty()) {
            auto* f = lv_label_create(col);
            lv_label_set_text(f, footer.c_str());
            lv_obj_set_style_max_width(f, 228, 0);
            lv_label_set_long_mode(f, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(f, small_font, 0);
            lv_obj_set_style_text_color(f, m.status == viewmodel::kMsgFailed ? colors.error
                                                                            : colors.text_disabled, 0);
            lv_obj_set_style_pad_hor(f, 3, 0);
        }
    }

    const int newer_count = thread_total_ - thread_start_ - static_cast<int>(thread_rows_.size());
    if (newer_count > 0) {
        auto* newer = lv_label_create(body_);
        lv_label_set_text_fmt(newer, "%d newer messages (Down)", newer_count);
        lv_obj_set_style_text_font(newer, small_font, 0);
        lv_obj_set_style_text_color(newer, colors.text_disabled, 0);
    }
    if (!any) {
        auto* empty = lv_label_create(body_);
        const std::string saved = view_model().conversation_draft(thread_key_);
        lv_label_set_text(empty, saved.empty() ? "No messages yet. Press Enter to write one." :
            ("Saved draft (edit on its original channel):\n" + saved).c_str());
        lv_obj_set_width(empty, lv_pct(95));
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(empty, small_font, 0);
        lv_obj_set_style_text_color(empty, colors.text_disabled, 0);
        lv_obj_center(empty);
        return;
    }

    lv_obj_update_layout(body_);
    if (msg_sel_ >= 0 && msg_sel_ < static_cast<int>(thread_rows_.size())) {
        lv_obj_scroll_to_view(thread_rows_[msg_sel_], LV_ANIM_OFF);
    } else if (at_bottom_) {
        lv_obj_scroll_to_y(body_, LV_COORD_MAX, LV_ANIM_OFF);
    } else
        lv_obj_scroll_to_y(body_, previous_scroll, LV_ANIM_OFF);
    view_model().mark_thread_read(thread_key_);   // viewing a thread clears its unread
}

void MessagesScreen::select_conversation(int index) {
    if (index < list_start_ || index >= list_start_ + static_cast<int>(conversation_rows_.size())) {
        selected_ = index;
        rebuild_list();
        return;
    }
    view::widgets::select_list_row(conversation_rows_, selected_ - list_start_, index - list_start_,
                                   view::palette(view_model().is_dark_mode()));
    selected_ = index;
    update_hint();
}

void MessagesScreen::select_message(int index) {
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto style = [&](int i, bool selected) {
        if (i < 0 || i >= static_cast<int>(thread_bubbles_.size()))
            return;
        auto *bubble = thread_bubbles_[i];
        lv_obj_set_style_border_width(bubble, selected ? 2 : 0, 0);
        lv_obj_set_style_border_color(bubble, colors.error, 0);
        lv_obj_set_style_border_side(bubble, LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_border_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(bubble, selected ? 5 : 7, 0);
    };
    style(msg_sel_, false);
    msg_sel_ = index;
    selected_message_ = index >= 0 ? thread_keys_[index] : "";
    style(msg_sel_, true);
    update_hint();
    if (index >= 0)
        lv_obj_scroll_to_view(thread_rows_[index], LV_ANIM_OFF);
}

void MessagesScreen::select_thread_message(int global_index) {
    int i = 0;
    for (const auto& message : view_model().messages()) {
        if (thread_of(message) != thread_key_) continue;
        if (i++ != global_index) continue;
        selected_message_ = message.from + "\n" + message.id;
        msg_sel_ = 0;
        at_bottom_ = false;
        rebuild_thread();
        return;
    }
}

void MessagesScreen::open_thread(const std::string& key, const std::string& peer_id,
                                 const std::string& peer_name, bool choose_channel) {
    mode_ = Mode::Thread;
    if (!from_search_) tab_ = viewmodel::is_group_thread(key) ? Tab::Channels : Tab::DirectMessages;
    thread_key_ = key;
    view_model().remember_conversation(key);
    peer_id_ = peer_id;
    peer_name_ = peer_name.empty() ? peer_name_for(peer_id) : peer_name;
    at_bottom_ = true;
    thread_start_ = -1;
    msg_sel_ = -1;
    dm_channel_ = -1;
    rebuild();
    if (choose_channel && !viewmodel::is_group_thread(thread_key_)) open_channel_picker();
}

// ---------------------------------------------------------------------------
// Node picker overlay (New message)
// ---------------------------------------------------------------------------
void MessagesScreen::open_picker() {
    selected_ = 0;
    picker_id_.clear();
    picker_name_.clear();
    picker_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(picker_);
    lv_obj_set_size(picker_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(picker_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(picker_, colors.background, 0);
    lv_obj_set_flex_flow(picker_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(picker_, 4, 0);
    lv_obj_set_style_pad_row(picker_, 2, 0);
    lv_obj_clear_flag(picker_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    view::widgets::style_list_scrollbar(picker_, colors);
    rebuild_picker();
}

void MessagesScreen::rebuild_picker() {
    const std::string selected_id =
        selected_ >= 0 && selected_ < static_cast<int>(picker_id_.size()) ? picker_id_[selected_] : "";
    lv_obj_clean(picker_);
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    auto* header = lv_label_create(picker_);
    lv_label_set_text(header, "New message to:");
    lv_obj_set_style_text_font(header, font, 0);
    lv_obj_set_style_text_color(header, colors.text, 0);

    picker_id_.clear(); picker_name_.clear();
    // Group channels first, so an empty history can still start a broadcast on a
    // specific (named) channel; then the nodes for direct messages.
    for (const auto& c : view_model().channels()) {
        picker_id_.push_back("ch:" + c.identity);
        picker_name_.push_back("# " + channel_name(c.index));
    }
    for (const auto& n : view_model().nodes()) {
        if (n.isSelf) continue;
        picker_id_.push_back(n.id);
        picker_name_.push_back(n.name.empty() ? n.id : n.name);
    }
    if (!selected_id.empty()) {
        const auto selected = std::find(picker_id_.begin(), picker_id_.end(), selected_id);
        selected_ = selected == picker_id_.end() ? -1 : static_cast<int>(selected - picker_id_.begin());
    }
    if (selected_ >= static_cast<int>(picker_id_.size())) selected_ = static_cast<int>(picker_id_.size()) - 1;

    lv_obj_t* sel = nullptr;
    for (int i = 0; i < static_cast<int>(picker_id_.size()); ++i) {
        auto* row = lv_label_create(picker_);
        lv_label_set_text(row, picker_name_[i].c_str());
        lv_obj_set_style_text_font(row, font, 0);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_pad_ver(row, 3, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        if (i == selected_) {
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(row, colors.button, 0);
            lv_obj_set_style_text_color(row, colors.primary, 0);
            sel = row;
        } else {
            lv_obj_set_style_text_color(row, colors.text, 0);
        }
    }
    if (picker_id_.empty()) {
        auto* none = lv_label_create(picker_);
        lv_label_set_text(none, "No other nodes to message yet.");
        lv_obj_set_style_text_font(none, font, 0);
        lv_obj_set_style_text_color(none, colors.text_disabled, 0);
    }
    lv_obj_update_layout(picker_);
    if (sel) lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
}

void MessagesScreen::close_picker() {
    if (picker_) {
        lv_obj_delete(picker_);
        picker_ = nullptr;
        // The recipient picker opens from New message. Its index is not a chat
        // selection; restore that row and apply updates received while covered.
        selected_ = 0;
        rebuild_list();
    }
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
void MessagesScreen::on_up() {
    if (composing_) return;
    if (overlay_) {   // scroll a long info/details/traceroute overlay
        const int32_t room = lv_obj_get_scroll_top(overlay_);
        if (room > 0) lv_obj_scroll_by(overlay_, 0, room < 40 ? room : 40, LV_ANIM_OFF);
        return;
    }
    if (menu_) {
        if (!menu_labels_.empty()) { menu_sel_ = (menu_sel_ + menu_labels_.size() - 1) % menu_labels_.size(); rebuild_menu(); }
        return;
    }
    if (picker_) {
        if (!picker_id_.empty()) { selected_ = (selected_ <= 0 ? static_cast<int>(picker_id_.size()) : selected_) - 1; rebuild_picker(); }
        return;
    }
    if (mode_ == Mode::List) {
        const int rows = list_offset() + static_cast<int>(conversations_.size());
        if (rows > 0) select_conversation((selected_ + rows - 1) % rows);
        return;
    }
    // Thread: dive from the compose row up into older messages.
    const int count = static_cast<int>(thread_rows_.size());
    if (count == 0) return;
    if (msg_sel_ < 0)
        select_message(count - 1);
    else if (msg_sel_ > 0)
        select_message(msg_sel_ - 1);
    else if (thread_start_ > 0)
        select_thread_message(thread_start_ - 1);
    at_bottom_ = false;
}

void MessagesScreen::on_down() {
    if (composing_) return;
    if (overlay_) {
        const int32_t room = lv_obj_get_scroll_bottom(overlay_);
        if (room > 0) lv_obj_scroll_by(overlay_, 0, room < 40 ? -room : -40, LV_ANIM_OFF);
        return;
    }
    if (menu_) {
        if (!menu_labels_.empty()) { menu_sel_ = (menu_sel_ + 1) % menu_labels_.size(); rebuild_menu(); }
        return;
    }
    if (picker_) {
        if (!picker_id_.empty()) { selected_ = (selected_ + 1) % picker_id_.size(); rebuild_picker(); }
        return;
    }
    if (mode_ == Mode::List) {
        const int rows = list_offset() + static_cast<int>(conversations_.size());
        if (rows > 0) select_conversation((selected_ + 1) % rows);
        return;
    }
    // Thread: come back toward the newest message, then to the compose row.
    const int count = static_cast<int>(thread_rows_.size());
    if (count == 0 || msg_sel_ < 0) return;
    if (msg_sel_ < count - 1)
        select_message(msg_sel_ + 1);
    else if (thread_start_ + count < thread_total_)
        select_thread_message(thread_start_ + count);
    else {
        select_message(-1);
        at_bottom_ = true;
        lv_obj_scroll_to_y(body_, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void MessagesScreen::on_enter() {
    if (overlay_) { close_info(); return; }
    if (menu_) { menu_activate(); return; }
    if (composing_) { commit_compose(); return; }
    if (picker_) {
        if (!picker_id_.empty() && selected_ >= 0 && selected_ < static_cast<int>(picker_id_.size())) {
            const std::string id = picker_id_[selected_];
            const std::string name = picker_name_[selected_];
            close_picker();
            if (viewmodel::is_group_thread(id)) {
                view_model().clear_dm_target();
                open_thread(id, "", "");          // a group channel thread
            } else {
                view_model().set_dm_target(id, name);
                open_thread("dm:" + id, id, name);
            }
        }
        return;
    }
    if (mode_ == Mode::List) {
        if (search_.empty() && selected_ == 0) { open_picker(); return; }
        const int i = selected_ - list_offset();
        if (i >= 0 && i < static_cast<int>(conversations_.size())) {
            // Copy the target: opening/rebuilding invalidates the list vectors.
            const Conversation target = conversations_[i];
            from_search_ = !search_.empty();
            if (viewmodel::is_group_thread(target.key)) view_model().clear_dm_target();
            else view_model().set_dm_target(target.peer, peer_name_for(target.peer));
            open_thread(target.key, target.peer, peer_name_for(target.peer), !from_search_);
            if (from_search_) {
                selected_message_ = target.message_key;
                msg_sel_ = 0;
                at_bottom_ = false;
                rebuild_thread();
            }
        }
        return;
    }
    // Thread mode.
    if (composing_) { commit_compose(); return; }
    if (msg_sel_ >= 0 && msg_sel_ < static_cast<int>(thread_msg_ids_.size())) {
        open_message_menu(thread_msg_ids_[msg_sel_]);
        return;
    }
    start_compose();
}

bool MessagesScreen::on_back() {
    if (overlay_) { close_info(); return true; }
    if (menu_) { close_menu(); return true; }
    if (composing_) { cancel_compose(); return true; }
    if (picker_) { close_picker(); return true; }
    if (mode_ == Mode::List && !search_.empty()) {
        search_.clear(); selected_ = 0; conversations_.clear(); rebuild_list(); return true;
    }
    if (mode_ == Mode::Thread) {
        if (msg_sel_ >= 0 && !from_search_) {
            select_message(-1);
            at_bottom_ = true;
            return true;
        }
        mode_ = Mode::List;
        from_search_ = false;
        view_model().clear_dm_target();
        rebuild();
        return true;
    }
    return false;   // list -> leave to the menu
}

bool MessagesScreen::on_key(uint32_t key) {
    if (composing_ || overlay_ || menu_ || picker_) return false;
    if (mode_ == Mode::List && search_.empty() && (key == LV_KEY_NEXT || key == LV_KEY_PREV)) {
        tab_ = tab_ == Tab::Channels ? Tab::DirectMessages : Tab::Channels;
        selected_ = 0;
        conversations_.clear();
        rebuild_list();
        return true;
    }
    if (mode_ == Mode::List && key == '/') {
        searching_ = composing_ = true;
        lv_textarea_set_placeholder_text(input_, "Search all messages");
        lv_textarea_set_text(input_, search_.c_str());
        lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(input_, LV_OBJ_FLAG_HIDDEN);
        if (auto* g = lv_group_get_default()) { lv_group_add_obj(g, input_); lv_group_focus_obj(input_); }
        return true;
    }
    if (mode_ == Mode::Thread && (key == 'o' || key == 'O')) {
        open_thread_menu();
        return true;
    }
    if (mode_ == Mode::Thread && (key == 'q' || key == 'Q')) {
        open_canned_picker();
        return true;
    }
    return false;
}
bool MessagesScreen::is_composing() { return composing_; }

// ---------------------------------------------------------------------------
// Compose
// ---------------------------------------------------------------------------
void MessagesScreen::start_compose() {
    if (!input_ || mode_ != Mode::Thread) return;
    if (send_channel() < 0) { open_channel_picker(); return; }
    lv_textarea_set_placeholder_text(input_, (view_model().channel(send_channel())->isPrivate ? "Message [Private channel]" : "Message [Public channel]"));
    lv_textarea_set_text(input_, view_model().draft(draft_key()).c_str());
    lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(input_, LV_OBJ_FLAG_HIDDEN);
    lv_group_t* g = lv_group_get_default();
    if (g) { lv_group_add_obj(g, input_); lv_group_focus_obj(input_); }
    composing_ = true;
}

void MessagesScreen::commit_compose() {
    if (!input_) return;
    const std::string value = lv_textarea_get_text(input_);
    if (searching_) {
        search_ = value; selected_ = 0; conversations_.clear();
        searching_ = composing_ = false; cancel_compose(); rebuild_list(); return;
    }
    const char* text = value.c_str();
    if (text && text[0] != '\0') {
        const std::string dest = (viewmodel::is_group_thread(thread_key_)) ? std::string() : peer_id_;
        at_bottom_ = true;
        view_model().save_draft(draft_key(), value);
        view_model().flush_drafts();
        if (!view_model().send_message(send_channel(), text, dest)) return;
        composing_ = false; // keep draft until the supervisor accepts the send
        lv_textarea_set_text(input_, "");
    }
    cancel_compose();
}

void MessagesScreen::cancel_compose() {
    if (!input_) return;
    if (composing_ && !searching_) { view_model().save_draft(draft_key(), lv_textarea_get_text(input_)); view_model().flush_drafts(); }
    composing_ = false;
    searching_ = false;
    lv_textarea_set_text(input_, "");
    lv_obj_add_flag(input_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    lv_group_t* g = lv_group_get_default();
    if (g) lv_group_remove_obj(input_);
    composing_ = false;
}

MessagesScreen::~MessagesScreen() {
    if (composing_ && !searching_) { view_model().save_draft(draft_key(), lv_textarea_get_text(input_)); view_model().flush_drafts(); }
}

std::string MessagesScreen::draft_key() const {
    return viewmodel::is_group_thread(thread_key_) ? thread_key_ : thread_key_ + "@" + dm_channel_id_;
}
void MessagesScreen::draft_changed(lv_event_t* event) {
    auto* self = static_cast<MessagesScreen*>(lv_event_get_user_data(event));
    if (self && self->composing_ && !self->searching_) self->view_model().save_draft(self->draft_key(), lv_textarea_get_text(self->input_));
}

// ---------------------------------------------------------------------------
// Action menus + read-only overlay (built on the same popup-list primitive)
// ---------------------------------------------------------------------------
const viewmodel::MessageInfo* MessagesScreen::message_by_id(const std::string& id) {
    if (id.empty()) return nullptr;
    for (const auto& m : view_model().messages()) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

std::string MessagesScreen::details_text(const viewmodel::MessageInfo& m) {
    const std::string historical =
        (m.channelName.empty() ? "Default" : m.channelName) +
        (m.channelIdentity.empty() ? " (legacy; privacy unknown)"
                                   : (m.channelPrivate ? " [Private]" : " [Public]"));
    std::string s;
    if (m.outgoing) {
        s += "To: " + (m.to == "broadcast" ? ("# " + historical) : peer_name_for(m.to)) + "\n";
    } else {
        s += "From: " + (m.fromName.empty() ? m.from : m.fromName) + "\n";
    }
    if (m.time > 0) {
        char t[40];
        fmt("%b %d, %Y  %H:%M:%S", t, sizeof(t), m.time);
        s += std::string("Time: ") + t + "\n";
    }
    s += std::string("Channel: ") + historical + "\n";
    if (m.outgoing && m.status != 0) {
        s += "Status: " + status_text(m.status) + "\n";
        if (m.status == viewmodel::kMsgDelivered)
            s += "The recipient radio confirmed receipt. This is not a read receipt.\n";
        else if (m.status == viewmodel::kMsgMesh)
            s += "The mesh acknowledged it; recipient receipt is not confirmed.\n";
        else if (m.status == viewmodel::kMsgSent)
            s += "Sent without a confirmed receipt.\n";
        else if (m.status == viewmodel::kMsgFailed)
            s += "Delivery was not confirmed. Check the reason before resending.\n";
    }
    if (m.hasSnr) {
        char b[24];
        std::snprintf(b, sizeof(b), "SNR: %.1f dB\n", m.snr);
        s += b;
    }
    if (m.hops >= 0) {
        s += "Hops: " + std::to_string(m.hops) + (m.hops == 0 ? " (direct)\n" : "\n");
    }
    if (!m.reason.empty()) s += "Reason: " + m.reason + "\n";
    if (!m.relay.empty()) {
        s += "Relayed by: " + m.relay + "\n";
    }
    return s;
}

void MessagesScreen::open_thread_menu() {
    if (mode_ != Mode::Thread) return;
    menu_kind_ = MenuKind::Thread;
    menu_target_ = thread_key_;
    menu_labels_.clear();
    menu_actions_.clear();
    if (view_model().unread_count(thread_key_) > 0) {
        menu_labels_.push_back("Mark as read"); menu_actions_.push_back(A_MARK_READ);
    }
    menu_labels_.push_back(view_model().is_muted(thread_key_) ? "Unmute" : "Mute");
    menu_actions_.push_back(A_MUTE);
    menu_labels_.push_back(view_model().pinned(thread_key_) ? "Unpin chat" : "Pin chat"); menu_actions_.push_back(A_PIN);
    menu_labels_.push_back("Send my location"); menu_actions_.push_back(A_SEND_LOCATION);
    if (viewmodel::is_group_thread(thread_key_)) {
        menu_labels_.push_back("Share channel (QR)"); menu_actions_.push_back(A_SHARE_CHANNEL);
    } else {
        menu_labels_.push_back("Choose DM channel"); menu_actions_.push_back(A_CHANNEL);
        menu_labels_.push_back("Request position"); menu_actions_.push_back(A_REQUEST_POSITION);
        menu_labels_.push_back("Trace route"); menu_actions_.push_back(A_TRACEROUTE);
    }
    menu_labels_.push_back("Clear history"); menu_actions_.push_back(A_CLEAR);
    menu_labels_.push_back("Delete conversation"); menu_actions_.push_back(A_DELETE_THREAD);
    menu_labels_.push_back("Conversation info"); menu_actions_.push_back(A_INFO);
    menu_sel_ = 0;

    menu_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(menu_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(menu_, colors.background, 0);
    lv_obj_set_flex_flow(menu_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(menu_, 4, 0);
    lv_obj_set_style_pad_row(menu_, 2, 0);
    lv_obj_clear_flag(menu_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_menu();
}

void MessagesScreen::open_message_menu(const std::string& msg_id) {
    const viewmodel::MessageInfo* m = message_by_id(msg_id);
    if (!m) return;
    menu_kind_ = MenuKind::Message;
    menu_target_ = msg_id;
    menu_labels_.clear();
    menu_actions_.clear();
    menu_labels_.push_back("Details"); menu_actions_.push_back(A_MSG_DETAILS);
    menu_labels_.push_back("Quote / edit"); menu_actions_.push_back(A_MSG_QUOTE);
    if (m->outgoing && m->status == viewmodel::kMsgFailed) {
        menu_labels_.push_back("Resend"); menu_actions_.push_back(A_MSG_RESEND);
    }
    menu_labels_.push_back("Delete message"); menu_actions_.push_back(A_MSG_DELETE);
    menu_sel_ = 0;

    menu_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(menu_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(menu_, colors.background, 0);
    lv_obj_set_flex_flow(menu_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(menu_, 4, 0);
    lv_obj_set_style_pad_row(menu_, 2, 0);
    lv_obj_clear_flag(menu_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_menu();
}

void MessagesScreen::open_canned_picker() {
    if (mode_ != Mode::Thread) return;
    if (send_channel() < 0) { open_channel_picker(); return; }
    const auto& canned = view_model().canned_messages();
    if (canned.empty()) {
        open_info("Quick messages",
                  "No quick messages yet.\nAdd them in Settings \xE2\x80\xBA Quick messages.");
        return;
    }
    menu_kind_ = MenuKind::Canned;
    menu_target_.clear();
    menu_labels_.clear();
    menu_actions_.clear();
    for (const auto& c : canned) { menu_labels_.push_back(c); menu_actions_.push_back(0); }
    menu_sel_ = 0;

    menu_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(menu_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(menu_, colors.background, 0);
    lv_obj_set_flex_flow(menu_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(menu_, 4, 0);
    lv_obj_set_style_pad_row(menu_, 2, 0);
    lv_obj_clear_flag(menu_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_menu();
}

void MessagesScreen::create_menu() {
    if (menu_) lv_obj_delete(menu_);
    menu_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(menu_);
    lv_obj_set_size(menu_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(menu_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(menu_, colors.background, 0);
    lv_obj_set_flex_flow(menu_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(menu_, 4, 0);
    lv_obj_set_style_pad_row(menu_, 2, 0);
    lv_obj_clear_flag(menu_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    rebuild_menu();
}

void MessagesScreen::open_channel_picker() {
    if (viewmodel::is_group_thread(thread_key_)) {
        view_model().apply_notice("Channel unavailable; choose a configured channel in New message");
        return;
    }
    close_menu();
    if (view_model().channels().empty()) { open_info("DM channel", "No enabled channels. Start the radio or add a channel first."); return; }
    menu_kind_ = MenuKind::Channel;
    channel_picker_ids_.clear();
    for (const auto& c : view_model().channels()) {
        channel_picker_ids_.push_back(c.identity);
        menu_labels_.push_back(channel_name(c.index));
        menu_actions_.push_back(c.index);
    }
    menu_sel_ = 0;
    create_menu();
}

void MessagesScreen::open_public_location_confirmation() {
    close_menu();
    location_channel_ = send_channel();
    location_peer_ = viewmodel::is_group_thread(thread_key_) ? std::string() : peer_id_;
    menu_kind_ = MenuKind::PublicLocation;
    menu_labels_ = {"Cancel", "Send precise coordinates"};
    menu_actions_ = {0, 1};
    menu_sel_ = 0;
    create_menu();
}

void MessagesScreen::rebuild_menu() {
    if (!menu_) return;
    lv_obj_clean(menu_);
    const auto colors = view::palette(view_model().is_dark_mode());
    view::widgets::style_list_scrollbar(menu_, colors);
    const lv_font_t* font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_14;

    auto* header = lv_label_create(menu_);
    std::string title = "Message";
    if (menu_kind_ == MenuKind::Thread) {
        title = viewmodel::is_group_thread(thread_key_) ? group_name(thread_key_) : peer_name_;
    } else if (menu_kind_ == MenuKind::Canned) {
        title = "Quick message";
    } else if (menu_kind_ == MenuKind::Channel) {
        title = "DM via channel:";
    } else if (menu_kind_ == MenuKind::DeleteConfirm) {
        title = deletion_action_ == A_CLEAR ? "Clear history? Draft stays.\nCannot undo."
              : deletion_action_ == A_DELETE_THREAD ? "Delete chat and draft?\nCannot undo."
              : "Delete saved message?\nOther radios keep their copies.\nCannot undo.";
    } else if (menu_kind_ == MenuKind::PublicLocation) {
        title = "Precise location via " + channel_name(location_channel_) + "?";
    }
    lv_label_set_text(header, title.c_str());
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_style_text_font(header, font, 0);
    lv_obj_set_style_text_color(header, colors.text, 0);

    lv_obj_t* sel = nullptr;
    for (int i = 0; i < static_cast<int>(menu_labels_.size()); ++i) {
        auto* row = lv_label_create(menu_);
        lv_label_set_text(row, menu_labels_[i].c_str());
        lv_obj_set_style_text_font(row, font, 0);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_pad_ver(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        const bool danger = (menu_actions_[i] == A_DELETE_THREAD || menu_actions_[i] == A_MSG_DELETE ||
                             menu_actions_[i] == A_CLEAR);
        if (i == menu_sel_) {
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(row, colors.button, 0);
            lv_obj_set_style_text_color(row, danger ? colors.error : colors.primary, 0);
            sel = row;
        } else {
            lv_obj_set_style_text_color(row, danger ? colors.error : colors.text, 0);
        }
    }
    lv_obj_update_layout(menu_);
    if (sel) lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
}

void MessagesScreen::close_menu() {
    if (menu_) { lv_obj_delete(menu_); menu_ = nullptr; }
    menu_kind_ = MenuKind::None;
    deletion_action_ = 0;
    menu_target_.clear();
    menu_labels_.clear();
    menu_actions_.clear();
    location_channel_ = -1;
    location_peer_.clear();
}

void MessagesScreen::confirm_deletion(int action, const std::string& target) {
    menu_kind_ = MenuKind::DeleteConfirm;
    deletion_action_ = action;
    menu_target_ = target; // stable identity; never a row index or vector pointer
    menu_labels_ = {"Cancel", action == A_CLEAR ? "Clear history" : "Delete"};
    menu_actions_ = {0, action};
    menu_sel_ = 0;
    create_menu();
}

void MessagesScreen::perform_deletion(int action, const std::string& target) {
    if (action == A_MSG_DELETE) {
        const auto* message = message_by_id(target);
        if (!message || thread_of(*message) != thread_key_) return;
        view_model().delete_message(target);
        msg_sel_ = -1;
    } else if (target == thread_key_) {
        if (action == A_CLEAR) {
            view_model().clear_thread(target);
            msg_sel_ = -1;
        } else if (action == A_DELETE_THREAD) {
            view_model().delete_thread(target);
            mode_ = Mode::List;
            view_model().clear_dm_target();
            selected_ = 0;
            msg_sel_ = -1;
            rebuild();
        }
    }
}

void MessagesScreen::menu_activate() {
    if (menu_kind_ == MenuKind::DeleteConfirm) {
        const int action = deletion_action_;
        const std::string target = menu_target_;
        const bool confirmed = menu_sel_ == 1;
        close_menu();
        if (confirmed) perform_deletion(action, target);
        return;
    }
    if (menu_kind_ == MenuKind::Channel) {
        const int channel = menu_sel_ >= 0 && menu_sel_ < static_cast<int>(menu_actions_.size()) ? menu_actions_[menu_sel_] : -1;
        const std::string identity = menu_sel_ >= 0 && menu_sel_ < static_cast<int>(channel_picker_ids_.size()) ? channel_picker_ids_[menu_sel_] : "";
        close_menu();
        dm_channel_id_ = identity;
        dm_channel_ = view_model().channel(channel) && view_model().channel(channel)->identity == identity ? channel : -1;
        update_hint();
        return;
    }
    if (menu_kind_ == MenuKind::PublicLocation) {
        const bool confirmed = menu_sel_ == 1;
        const int channel = location_channel_;
        const std::string dest = location_peer_;
        close_menu();
        if (confirmed && channel >= 0 && channel == send_channel() &&
            dest == (viewmodel::is_group_thread(thread_key_) ? std::string() : peer_id_))
            view_model().send_position(channel, dest, true);
        return;
    }
    // Quick messages: the selected label IS the text; send it straight away.
    if (menu_kind_ == MenuKind::Canned) {
        if (menu_sel_ < 0 || menu_sel_ >= static_cast<int>(menu_labels_.size())) { close_menu(); return; }
        const std::string text = menu_labels_[menu_sel_];
        close_menu();
        if (mode_ == Mode::Thread && !text.empty()) {
            const std::string dest = (viewmodel::is_group_thread(thread_key_)) ? std::string() : peer_id_;
            at_bottom_ = true;
            msg_sel_ = -1;
            view_model().send_message(send_channel(), text, dest);
        }
        return;
    }
    if (menu_sel_ < 0 || menu_sel_ >= static_cast<int>(menu_actions_.size())) return;
    const int action = menu_actions_[menu_sel_];
    const std::string target = menu_target_;
    close_menu();

    switch (action) {
        case A_MARK_READ:
            view_model().mark_thread_read(target);
            break;
        case A_PIN: view_model().toggle_pin(target); break;
        case A_MUTE:
            view_model().toggle_mute(target);
            break;
        case A_CLEAR:
        case A_DELETE_THREAD:
            confirm_deletion(action, target);
            break;
        case A_INFO: {
            std::string body;
            if (viewmodel::is_group_thread(thread_key_)) {
                const std::string cn = channel_name(view_model().channel_for_thread(thread_key_));
                body = "Group channel: " + cn + "\n"
                       "Messages here reach everyone on the " + cn + " channel.";
            } else {
                body = "Direct message\nNode: " + peer_name_ + "\nID: " + peer_id_;
                for (const auto& n : view_model().nodes()) {
                    if (n.id == peer_id_) {
                        if (n.hasSnr) { char b[24]; std::snprintf(b, sizeof(b), "\nSNR: %.1f dB", n.snr); body += b; }
                        if (n.hops >= 0) body += "\nHops: " + std::to_string(n.hops);
                        break;
                    }
                }
            }
            body += "\n\nHistory is kept on this device only.";
            open_info(viewmodel::is_group_thread(thread_key_) ? group_name(thread_key_) : peer_name_, body);
            break;
        }
        case A_MSG_DETAILS: {
            const viewmodel::MessageInfo* m = message_by_id(target);
            if (m) open_info("Message details", details_text(*m));
            break;
        }
        case A_MSG_QUOTE: {
            const viewmodel::MessageInfo* m = message_by_id(target);
            if (m && mode_ == Mode::Thread) {
                const std::string text = m->text;
                msg_sel_ = -1;
                start_compose();
                if (composing_ && input_) {
                    lv_textarea_set_text(input_, text.c_str());
                    lv_textarea_set_cursor_pos(input_, LV_TEXTAREA_CURSOR_LAST);
                }
            }
            break;
        }
        case A_MSG_RESEND:
            view_model().resend_message(target);
            break;
        case A_MSG_DELETE:
            confirm_deletion(action, target);
            break;
        case A_SEND_LOCATION: {
            const int channel = send_channel();
            if (channel < 0) { open_channel_picker(); break; }
            if (!view_model().channel(channel)->isPrivate) { open_public_location_confirmation(); break; }
            const std::string dest = (viewmodel::is_group_thread(thread_key_)) ? std::string() : peer_id_;
            at_bottom_ = true;
            msg_sel_ = -1;
            view_model().send_position(channel, dest);
            break;
        }
        case A_CHANNEL:
            open_channel_picker();
            break;
        case A_REQUEST_POSITION:
            if (send_channel() < 0) { open_channel_picker(); break; }
            view_model().request_position(peer_id_, send_channel());
            open_info("Request position", "Asked " + peer_name_ + " for their position.\n"
                                          "It updates on the Nodes and Map screens when it replies.");
            break;
        case A_TRACEROUTE:
            if (send_channel() < 0) { open_channel_picker(); break; }
            open_info("Trace route", "Tracing the route to " + peer_name_ + "\xE2\x80\xA6\n"
                                     "(up to ~20 seconds)");
            awaiting_traceroute_ = true;
            view_model().traceroute(peer_id_, send_channel());
            break;
        case A_SHARE_CHANNEL:
            open_info("Share channel", "Building the selected channel share code...");
            awaiting_share_ = true;
            view_model().request_channel_share(send_channel());
            break;
        default:
            break;
    }
}

void MessagesScreen::open_info(const std::string& title, const std::string& body) {
    close_info();
    overlay_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(overlay_);
    lv_obj_set_size(overlay_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(overlay_, 6, 0);
    lv_obj_set_style_pad_row(overlay_, 4, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    view::widgets::style_list_scrollbar(overlay_, colors);

    const lv_font_t* title_font = assets().load_standard_font(14);
    if (!title_font) title_font = &lv_font_montserrat_14;
    const lv_font_t* body_font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    if (!body_font) body_font = &lv_font_montserrat_12;

    auto* h = lv_label_create(overlay_);
    lv_label_set_text(h, title.c_str());
    lv_obj_set_width(h, lv_pct(100));
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(h, title_font, 0);
    lv_obj_set_style_text_color(h, colors.primary, 0);

    auto* b = lv_label_create(overlay_);
    lv_label_set_text(b, body.c_str());
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_text_font(b, body_font, 0);
    lv_obj_set_style_text_color(b, colors.text, 0);


}

void MessagesScreen::close_info() {
    share_visible_ = false;
    awaiting_share_ = false;
    awaiting_traceroute_ = false;
    view_model().cancel_channel_share();
    view_model().cancel_traceroute();
    if (overlay_) { lv_obj_delete(overlay_); overlay_ = nullptr; }
}

void MessagesScreen::open_qr(const std::string& title, const std::string& url) {
    close_info();
    share_visible_ = true;
    if (url.empty()) {
        open_info(title, "Could not build a share code.");
        return;
    }
    overlay_ = lv_obj_create(lv_obj_get_parent(body_));
    lv_obj_remove_style_all(overlay_);
    lv_obj_set_size(overlay_, lv_pct(100), lv_pct(100));
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(overlay_, colors.background, 0);
    lv_obj_set_flex_flow(overlay_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(overlay_, 4, 0);
    lv_obj_set_style_pad_row(overlay_, 3, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    view::widgets::style_list_scrollbar(overlay_, colors);

    const lv_font_t* font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    if (!font) font = &lv_font_montserrat_12;

    auto* h = lv_label_create(overlay_);
    lv_label_set_text(h, ("Scan to join " + title).c_str());
    lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_height(h, lv_font_get_line_height(font));
    lv_label_set_long_mode(h, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(h, font, 0);
    lv_obj_set_style_text_color(h, colors.primary, 0);

    // White quiet-zone box so a camera can read the code in either theme.
    auto* box = lv_obj_create(overlay_);
    lv_obj_remove_style_all(box);
    // Title + gap + quiet-zone box + padding must fit below the header (140px).
    lv_obj_set_size(box, 110, 110);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_white(), 0);
    lv_obj_set_style_pad_all(box, 5, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto* qr = lv_qrcode_create(box);
    lv_qrcode_set_size(qr, 100);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    if (lv_qrcode_update(qr, url.c_str(), static_cast<uint32_t>(url.size())) != LV_RESULT_OK)
        open_info(title, "Could not render this share code. Try sharing a single channel.");


}

void MessagesScreen::traceroute_cb(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<MessagesScreen*>(lv_observer_get_user_data(observer));
    if (!self || !self->awaiting_traceroute_) return;   // ignore the subscribe-time call
    self->awaiting_traceroute_ = false;
    self->open_info("Trace route", self->view_model().traceroute_result());
}

void MessagesScreen::channel_share_cb(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<MessagesScreen*>(lv_observer_get_user_data(observer));
    if (!self || !self->awaiting_share_) return;        // ignore the subscribe-time call
    self->awaiting_share_ = false;
    self->open_qr(self->view_model().channel_share_name(), self->view_model().channel_share_url());
}

} // namespace screen
