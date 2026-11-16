/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "json.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>
#include <utility>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      greeting_subject_(model_.greeting()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      counter_subject_(0),
      bold_text_subject_(false),
      info_visible_subject_(false),
      quit_requested_subject_(false) {
    // A per-session prefix so our client ids never collide with those from an
    // earlier UI session (the supervisor keeps history across UI reconnects).
    std::random_device rd;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "c%08x%08x%08x%08x-", rd(), rd(), rd(), rd());
    session_id_ = buf;
}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::greeting_subject() {
    return greeting_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::counter_subject() {
    return counter_subject_.native();
}

lv_subject_t* BaseViewModel::bold_text_subject() {
    return bold_text_subject_.native();
}

lv_subject_t* BaseViewModel::info_visible_subject() {
    return info_visible_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
    persist();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_messages_page() {
    model_.set_current_page(model::AppPage::Messages);
    publish_all();
}

void BaseViewModel::show_nodes_page() {
    model_.set_current_page(model::AppPage::Nodes);
    publish_all();
}

void BaseViewModel::show_map_page() {
    model_.set_current_page(model::AppPage::Map);
    publish_all();
}

void BaseViewModel::show_settings_page() {
    model_.set_current_page(model::AppPage::Settings);
    publish_all();
}

void BaseViewModel::toggle_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::show_next_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::show_prev_page() {
    model_.prev_page();
    publish_all();
}

void BaseViewModel::show_menu_page() {
    model_.set_current_page(model::AppPage::Menu);
    publish_all();
}

lv_subject_t* BaseViewModel::menu_index_subject() {
    return menu_index_subject_.native();
}

void BaseViewModel::menu_prev() {
    const int i = (menu_index_subject_.value() + model::kMenuEntryCount - 1) % model::kMenuEntryCount;
    menu_index_subject_.set(i);
}

void BaseViewModel::menu_next() {
    const int i = (menu_index_subject_.value() + 1) % model::kMenuEntryCount;
    menu_index_subject_.set(i);
}

void BaseViewModel::open_menu_selection() {
    switch (menu_index_subject_.value()) {
        case 1: show_nodes_page(); break;
        case 2: show_map_page(); break;
        case 3: show_settings_page(); break;
        case 0:
        default: show_messages_page(); break;
    }
}

void BaseViewModel::increment_counter() {
    counter_subject_.set(counter_subject_.value() + 1);
}

void BaseViewModel::decrement_counter() {
    const auto next = counter_subject_.value() - 1;
    counter_subject_.set(next < 0 ? 0 : next);
}

void BaseViewModel::toggle_bold_text() {
    bold_text_subject_.toggle();
}

void BaseViewModel::toggle_info() {
    info_visible_subject_.toggle();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    greeting_subject_.set(model_.greeting());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

// --- mesh state subject accessors -------------------------------------------
lv_subject_t* BaseViewModel::radio_state_subject() { return radio_state_subject_.native(); }
lv_subject_t* BaseViewModel::region_subject() { return region_subject_.native(); }
lv_subject_t* BaseViewModel::owner_subject() { return owner_subject_.native(); }
lv_subject_t* BaseViewModel::connected_subject() { return connected_subject_.native(); }
lv_subject_t* BaseViewModel::nodes_version_subject() { return nodes_version_subject_.native(); }
lv_subject_t* BaseViewModel::channels_version_subject() { return channels_version_subject_.native(); }
lv_subject_t* BaseViewModel::messages_version_subject() { return messages_version_subject_.native(); }
lv_subject_t* BaseViewModel::unread_version_subject() { return unread_version_subject_.native(); }
lv_subject_t* BaseViewModel::incoming_message_subject() { return incoming_message_subject_.native(); }
lv_subject_t* BaseViewModel::traceroute_subject() { return traceroute_subject_.native(); }
lv_subject_t* BaseViewModel::channel_share_subject() { return channel_share_subject_.native(); }

// --- applied as supervisor events arrive (UI thread) ------------------------
void BaseViewModel::set_connected(bool connected) {
    connected_subject_.set(connected);
    if (connected) send_intent("{\"intent\":\"message.retention\",\"cap\":" + std::to_string(retention_cap_) + "}");
    if (!connected) {
        radio_state_subject_.set("unknown");
        config_unknown();
        cancel_channel_share();
        cancel_traceroute();
    }
}

void BaseViewModel::apply_status(const std::string& radio, const std::string& region,
                                 const std::string& owner) {
    if (radio != "online") config_unknown();
    radio_state_subject_.set(radio.c_str());
    region_subject_.set(region.c_str());
    owner_subject_.set(owner.c_str());
}

void BaseViewModel::set_nodes(std::vector<NodeInfo> nodes) {
    if (nodes.size() > 256) nodes.resize(256);
    for (auto& n : nodes) {
        if (!std::isfinite(n.lat) || !std::isfinite(n.lon) ||
            n.lat < -90 || n.lat > 90 || n.lon < -180 || n.lon > 180) n.hasPos = false;
    }
    nodes_ = std::move(nodes);
    apply_favorites();
    nodes_version_subject_.set(nodes_version_subject_.value() + 1);
}

void BaseViewModel::apply_favorites() {
    for (auto& n : nodes_) {
        n.favorite = favorites_.count(n.id) != 0;
    }
}

bool BaseViewModel::is_favorite(const std::string& id) const {
    return favorites_.count(id) != 0;
}

void BaseViewModel::toggle_favorite(const std::string& id) {
    if (id.empty()) return;
    if (favorites_.count(id)) {
        favorites_.erase(id);
    } else {
        favorites_.insert(id);
    }
    apply_favorites();
    nodes_version_subject_.set(nodes_version_subject_.value() + 1);
    persist();
}

std::vector<std::string> BaseViewModel::favorites() const {
    return std::vector<std::string>(favorites_.begin(), favorites_.end());
}

void BaseViewModel::load_favorites(const std::vector<std::string>& ids) {
    favorites_.clear();
    for (const auto& id : ids) {
        if (!id.empty()) favorites_.insert(id);
    }
    apply_favorites();
    nodes_version_subject_.set(nodes_version_subject_.value() + 1);
}

void BaseViewModel::set_channels(std::vector<ChannelInfo> channels) {
    if (channels.size() > 8) channels.resize(8);
    std::set<int> indices;
    channels.erase(std::remove_if(channels.begin(), channels.end(), [&](const auto& ch) {
        return ch.index < 0 || ch.index > 7 || (ch.role != "PRIMARY" && ch.role != "SECONDARY") || !indices.insert(ch.index).second;
    }), channels.end());
    channels_ = std::move(channels);
    if (!share_channel_valid()) { cancel_channel_share(); channel_share_url_.clear(); }
    channels_version_subject_.set(channels_version_subject_.value() + 1);
}

void BaseViewModel::set_messages(std::vector<MessageInfo> messages) {
    if (messages.size() > 2000) messages.erase(messages.begin(), messages.end() - 2000);
    messages_ = std::move(messages);
    messages_version_subject_.set(messages_version_subject_.value() + 1);
    trim_all_threads();
    prune_unread();
}

void BaseViewModel::add_message(MessageInfo message) {
    if (message.outgoing) {
        const auto it = sent_draft_keys_.find(message.id);
        if (it != sent_draft_keys_.end()) {
            if (draft(it->second) == message.text) { save_draft(it->second, ""); flush_drafts(); }
            sent_draft_keys_.erase(it);
        }
    }
    for (auto& m : messages_) {
        if (!message.id.empty() && m.id == message.id && m.from == message.from) {
            m = std::move(message);
            messages_version_subject_.set(messages_version_subject_.value() + 1);
            return;
        }
    }
    const bool incoming = !message.outgoing;
    const std::string key = thread_key_of(message);
    if (incoming) {
        last_incoming_from_ = message.fromName.empty() ? message.from : message.fromName;
        last_incoming_text_ = message.text;
        last_incoming_thread_ = key;
    }

    // Bump unread BEFORE the version change: the version change triggers the
    // thread view to re-render and mark the visible thread read, which must see
    // (and clear) this increment, or an already-visible message stays unread.
    if (incoming && !is_muted(key) && !mute_all_ && !quiet_now()) {
        unread_[key] += 1;
        unread_version_subject_.set(unread_version_subject_.value() + 1);
        incoming_message_subject_.set(incoming_message_subject_.value() + 1);
    }

    messages_.push_back(std::move(message));
    if (messages_.size() > 2000) messages_.erase(messages_.begin());
    prune_unread();
    messages_version_subject_.set(messages_version_subject_.value() + 1);
    trim_thread(key);   // honour the per-thread retention cap
}

void BaseViewModel::prune_unread() {
    std::set<std::string> live;
    for (const auto& m : messages_) live.insert(thread_key_of(m));
    bool changed = false;
    for (auto it = unread_.begin(); it != unread_.end();) {
        if (!live.count(it->first)) { it = unread_.erase(it); changed = true; }
        else ++it;
    }
    if (changed) unread_version_subject_.set(unread_version_subject_.value() + 1);
}

// --- user actions -> intents ------------------------------------------------
void BaseViewModel::set_command_sink(std::function<bool(const std::string&)> sink) {
    command_sink_ = std::move(sink);
}

bool BaseViewModel::send_intent(const std::string& line) {
    if (command_sink_ && command_sink_(line)) return true;
    apply_notice("Supervisor unavailable; action was not sent");
    return false;
}

void BaseViewModel::radio_start() { send_intent("{\"intent\":\"radio.start\"}"); }
void BaseViewModel::radio_stop() { send_intent("{\"intent\":\"radio.stop\"}"); }

bool BaseViewModel::emit_message_send(int channel, const std::string& text,
                                      const std::string& dest, const std::string& cid) {
    std::string line = "{\"intent\":\"message.send\",\"channel\":" + std::to_string(channel) +
                       ",\"text\":\"" + json::escape(text) + "\",\"cid\":\"" + cid + "\"" +
                       ",\"wantAck\":" + (want_ack_ ? "true" : "false");
    if (!dest.empty()) {
        line += ",\"dest\":\"" + json::escape(dest) + "\"";
    }
    line += channel_fields(channel) + "}";
    return send_intent(line);
}

bool BaseViewModel::send_message(int channel, const std::string& text, const std::string& dest) {
    if (text.empty() || text.size() > 200) {
        apply_notice("Messages must contain 1..200 UTF-8 bytes");
        return false;
    }
    if (!this->channel(channel) || this->channel(channel)->identity.empty()) { apply_notice("Channel unavailable; reopen the conversation"); return false; }
    const std::string cid = session_id_ + std::to_string(++cid_counter_);
    const bool accepted = emit_message_send(channel, text, dest, cid);

    // Local echo is replaced by authoritative supervisor acceptance/delivery.
    MessageInfo m;
    m.from = "self";
    m.fromName = "me";
    m.to = dest.empty() ? "broadcast" : dest;
    m.text = text;
    m.id = cid;
    m.channel = channel;
    m.channelIdentity = this->channel(channel)->identity;
    m.channelName = this->channel(channel)->name;
    m.channelPrivate = this->channel(channel)->isPrivate;
    m.outgoing = true;
    m.status = !accepted ? kMsgFailed : (want_ack_ ? kMsgSending : 0);   // no ack requested -> no pending state
    m.time = static_cast<long>(std::time(nullptr));
    add_message(std::move(m));
    if (accepted) {
        if (sent_draft_keys_.size() >= 2000) sent_draft_keys_.erase(sent_draft_keys_.begin());
        sent_draft_keys_[cid] = dest.empty() ? "ch:" + this->channel(channel)->identity : "dm:" + dest + "@" + this->channel(channel)->identity;
    }
    return accepted;
}

void BaseViewModel::set_delivery(const std::string& id, int status, int hops, const std::string& reason) {
    if (id.empty()) return;
    bool changed = false;
    for (auto& m : messages_) {
        if (m.outgoing && m.id == id) {
            m.status = status;
            m.reason = reason;
            if (hops >= 0) m.hops = hops;
            changed = true;
        }
    }
    if (changed) {
        messages_version_subject_.set(messages_version_subject_.value() + 1);
    }
}

// --- per-thread / per-message state ----------------------------------------
int BaseViewModel::unread_count(const std::string& key) const {
    const auto it = unread_.find(key);
    return it == unread_.end() ? 0 : it->second;
}

int BaseViewModel::total_unread() const {
    int total = 0;
    for (const auto& [key, n] : unread_) total += n;
    return total;
}

void BaseViewModel::mark_thread_read(const std::string& key) {
    const auto it = unread_.find(key);
    if (it != unread_.end() && it->second != 0) {
        unread_.erase(it);
        unread_version_subject_.set(unread_version_subject_.value() + 1);
    }
}

bool BaseViewModel::is_muted(const std::string& key) const {
    return muted_.count(key) != 0;
}

void BaseViewModel::toggle_mute(const std::string& key) {
    if (muted_.count(key)) muted_.erase(key);
    else muted_.insert(key);
    persist();
}

void BaseViewModel::clear_thread(const std::string& key) {
    const auto before = messages_.size();
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(),
                                   [&](const MessageInfo& m) { return thread_key_of(m) == key; }),
                    messages_.end());
    mark_thread_read(key);
    // Clear it in the authoritative store too, or it returns on reconnect.
    send_intent("{\"intent\":\"message.clear\",\"thread\":\"" + json::escape(key) + "\"}");
    if (messages_.size() != before) {
        messages_version_subject_.set(messages_version_subject_.value() + 1);
    }
}

void BaseViewModel::delete_thread(const std::string& key) {
    clear_thread(key);
    for (auto it = drafts_.begin(); it != drafts_.end();) {
        if (it->first == key || it->first.rfind(key + "@", 0) == 0) { dirty_drafts_.erase(it->first); it = drafts_.erase(it); }
        else ++it;
    }
    pinned_.erase(key);
    send_intent("{\"intent\":\"thread.delete\",\"thread\":\"" + json::escape(key) + "\"}");
    muted_.erase(key);   // forget everything local about the conversation
    persist();
}

void BaseViewModel::delete_message(const std::string& id) {
    if (id.empty()) return;
    const auto before = messages_.size();
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(),
                                   [&](const MessageInfo& m) { return m.id == id; }),
                    messages_.end());
    send_intent("{\"intent\":\"message.delete\",\"id\":\"" + json::escape(id) + "\"}");
    if (messages_.size() != before) {
        messages_version_subject_.set(messages_version_subject_.value() + 1);
    }
}

void BaseViewModel::resend_message(const std::string& id) {
    for (auto& m : messages_) {
        if (m.outgoing && m.id == id) {
            const ChannelInfo* current = nullptr;
            for (const auto& candidate : channels_) if (candidate.identity == m.channelIdentity && !m.channelIdentity.empty()) { current = &candidate; break; }
            if (!current) { apply_notice("Original channel changed; retry cancelled"); return; }
            m.channel = current->index;
            m.status = kMsgSending;
            const std::string dest = (m.to == "broadcast" || m.to.empty()) ? std::string() : m.to;
            if (!emit_message_send(m.channel, m.text, dest, m.id)) m.status = kMsgFailed;   // same cid -> updates this bubble
            messages_version_subject_.set(messages_version_subject_.value() + 1);
            return;
        }
    }
}

void BaseViewModel::persist() {
    if (on_persist_) on_persist_();
}

void BaseViewModel::add_canned(const std::string& text) {
    if (!text.empty()) { canned_.push_back(text); persist(); }
}

void BaseViewModel::update_canned(int index, const std::string& text) {
    if (index < 0 || index >= static_cast<int>(canned_.size())) return;
    if (text.empty()) canned_.erase(canned_.begin() + index);
    else canned_[index] = text;
    persist();
}

void BaseViewModel::remove_canned(int index) {
    if (index < 0 || index >= static_cast<int>(canned_.size())) return;
    canned_.erase(canned_.begin() + index);
    persist();
}

void BaseViewModel::set_show_signal(bool on) {
    if (show_signal_ == on) return;
    show_signal_ = on;
    persist();
    messages_version_subject_.set(messages_version_subject_.value() + 1);  // re-render bubbles
}

void BaseViewModel::set_retention_cap(int cap) {
    cap = std::clamp(cap, 0, 2000);
    if (retention_cap_ == cap) return;
    retention_cap_ = cap;
    persist();
    send_intent("{\"intent\":\"message.retention\",\"cap\":" + std::to_string(cap) + "}");
    trim_all_threads();
}

void BaseViewModel::set_mute_all(bool on) {
    if (mute_all_ == on) return;
    mute_all_ = on;
    persist();
}

void BaseViewModel::set_want_ack(bool on) {
    if (want_ack_ == on) return;
    want_ack_ = on;
    persist();
}

void BaseViewModel::send_position(int channel, const std::string& dest, bool public_confirmed) {
    std::string line = "{\"intent\":\"position.send\",\"channel\":" + std::to_string(channel);
    if (!dest.empty()) line += ",\"dest\":\"" + json::escape(dest) + "\"";
    line += channel_fields(channel);
    line += std::string(",\"public_confirmed\":") + (public_confirmed ? "true}" : "false}");
    send_intent(line);
}

void BaseViewModel::request_position(const std::string& node_id, int channel) {
    if (node_id.empty()) return;
    send_intent("{\"intent\":\"position.request\",\"node\":\"" + json::escape(node_id) + "\",\"channel\":" + std::to_string(channel) + channel_fields(channel) + "}");
}

void BaseViewModel::traceroute(const std::string& node_id, int channel) {
    if (node_id.empty()) return;
    trace_node_ = node_id;
    trace_request_ = session_id_ + "trace" + std::to_string(++cid_counter_);
    if (!send_intent("{\"intent\":\"traceroute\",\"node\":\"" + json::escape(node_id) +
                    "\",\"id\":\"" + trace_request_ + "\",\"channel\":" +
                    std::to_string(channel) + channel_fields(channel) + "}")) cancel_traceroute();
}

const ChannelInfo* BaseViewModel::channel(int index) const {
    for (const auto& c : channels_) if (c.index == index) return &c;
    return nullptr;
}

std::string BaseViewModel::channel_label(int index) const {
    const auto* c = channel(index);
    if (!c) return "Unavailable channel " + std::to_string(index);
    return std::to_string(index) + ": " + (c->name.empty() ? "Default" : c->name) + (c->isPrivate ? " [Private]" : " [Public]");
}

void BaseViewModel::request_channel_share(int channel) {
    share_channel_ = channel;
    const auto* selected = this->channel(channel);
    share_identity_ = selected ? selected->identity : "";
    share_request_ = session_id_ + "share" + std::to_string(++cid_counter_);
    if (!send_intent("{\"intent\":\"channel.share\",\"channel\":" + std::to_string(channel) +
                    ",\"id\":\"" + share_request_ + "\"" + channel_fields(channel) + "}")) cancel_channel_share();
}

bool BaseViewModel::share_channel_valid() const {
    const auto* selected = channel(share_channel_);
    return selected && !share_identity_.empty() && selected->identity == share_identity_;
}

void BaseViewModel::apply_traceroute(bool ok, const std::string& text, const std::string& node, const std::string& request) {
    if (trace_request_.empty() || trace_request_ != request || trace_node_ != node) return;
    trace_request_.clear();
    traceroute_result_ = ok ? text : (text.empty() ? "No route found." : text);
    traceroute_subject_.set(traceroute_subject_.value() + 1);
}

void BaseViewModel::apply_channel_share(int channel, const std::string& name,
                                        const std::string& url, const std::string& request, const std::string& identity) {
    if (share_request_.empty() || request != share_request_ || channel != share_channel_) return;
    if (identity != share_identity_ || !share_channel_valid()) return;
    share_request_.clear();
    channel_share_name_ = name;
    channel_share_url_ = url;
    channel_share_subject_.set(channel_share_subject_.value() + 1);
}

lv_subject_t* BaseViewModel::notice_subject() { return notice_subject_.native(); }

void BaseViewModel::apply_notice(const std::string& text) {
    last_notice_ = text;
    notice_subject_.set(notice_subject_.value() + 1);
}

void BaseViewModel::load_canned(std::vector<std::string> list) {
    canned_ = std::move(list);
}

void BaseViewModel::load_muted(std::vector<std::string> keys) {
    muted_.clear();
    for (auto& k : keys) muted_.insert(std::move(k));
}

void BaseViewModel::load_messaging_prefs(bool show_signal, int retention, bool mute_all, bool want_ack) {
    show_signal_ = show_signal;
    retention_cap_ = std::clamp(retention, 0, 2000);
    mute_all_ = mute_all;
    want_ack_ = want_ack;
}

void BaseViewModel::trim_thread(const std::string& key) {
    if (retention_cap_ <= 0) return;
    int total = 0;
    for (const auto& m : messages_) if (thread_key_of(m) == key) ++total;
    int remove = total - retention_cap_;
    if (remove <= 0) return;
    std::vector<MessageInfo> kept;
    kept.reserve(messages_.size());
    int skipped = 0;
    for (auto& m : messages_) {
        if (thread_key_of(m) == key && skipped < remove) { ++skipped; continue; }
        kept.push_back(std::move(m));
    }
    messages_.swap(kept);
    messages_version_subject_.set(messages_version_subject_.value() + 1);
}

void BaseViewModel::trim_all_threads() {
    if (retention_cap_ <= 0) return;
    std::set<std::string> keys;
    for (const auto& m : messages_) keys.insert(thread_key_of(m));
    for (const auto& k : keys) trim_thread(k);
}

void BaseViewModel::set_dm_target(const std::string& id, const std::string& name) {
    dm_target_id_ = id;
    dm_target_name_ = name;
}

void BaseViewModel::clear_dm_target() {
    dm_target_id_.clear();
    dm_target_name_.clear();
}

void BaseViewModel::add_channel_url(const std::string& url) {
    send_intent("{\"intent\":\"channel.add\",\"url\":\"" + json::escape(url) + "\"}");
}

void BaseViewModel::scan_channel_qr() {
    send_intent("{\"intent\":\"channel.scan\"}");
}

void BaseViewModel::set_region(const std::string& region) {
    send_intent("{\"intent\":\"region.set\",\"region\":\"" + json::escape(region) + "\"}");
}

void BaseViewModel::set_owner(const std::string& name, const std::string& short_name) {
    send_intent("{\"intent\":\"owner.set\",\"name\":\"" + json::escape(name) +
                "\",\"short\":\"" + json::escape(short_name) + "\"}");
}

// --- device settings --------------------------------------------------------
lv_subject_t* BaseViewModel::modem_preset_subject() { return modem_preset_subject_.native(); }
lv_subject_t* BaseViewModel::hop_limit_subject() { return hop_limit_subject_.native(); }
lv_subject_t* BaseViewModel::position_subject() { return position_subject_.native(); }
lv_subject_t* BaseViewModel::mqtt_subject() { return mqtt_subject_.native(); }
lv_subject_t* BaseViewModel::autostart_subject() { return autostart_subject_.native(); }
lv_subject_t* BaseViewModel::node_id_subject() { return node_id_subject_.native(); }
lv_subject_t* BaseViewModel::firmware_subject() { return firmware_subject_.native(); }
lv_subject_t* BaseViewModel::model_subject() { return model_subject_.native(); }

void BaseViewModel::config_unknown() {
    config_request_.clear();
    config_state_.set("unknown");
}

void BaseViewModel::tick() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    if (!dirty_drafts_.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now).count() >= draft_deadline_) flush_drafts();
    if (!config_request_.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now).count() > config_deadline_) {
        config_unknown();
        apply_notice("Radio confirmation timed out; settings unknown");
    }
}

void BaseViewModel::send_config(const std::string& fields) {
    if (!connected_subject_.value() || std::string(radio_state_subject_.value()) != "online") {
        apply_notice("Turn the radio on before changing settings");
        return;
    }
    if (!config_request_.empty()) { apply_notice("Waiting for radio confirmation"); return; }
    config_request_ = session_id_ + "config" + std::to_string(++cid_counter_);
    config_deadline_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() + 45000;
    config_state_.set("pending");
    if (!send_intent("{\"intent\":\"config.set\",\"id\":\"" + config_request_ + "\"," + fields + "}")) config_unknown();
}

void BaseViewModel::set_modem_preset(const std::string& preset) {
    send_config("\"path\":\"lora.modem_preset\",\"value\":\"" + json::escape(preset) + "\"");
}

void BaseViewModel::set_hop_limit(int hops) {
    send_config("\"path\":\"lora.hop_limit\",\"value\":" + std::to_string(hops));
}

void BaseViewModel::set_frequency_slot(int slot) {
    send_config("\"path\":\"lora.channel_num\",\"value\":" + std::to_string(slot));
}

void BaseViewModel::set_position_sharing(bool on, int channel, bool public_confirmed) {
    send_config(std::string("\"path\":\"position.enabled\",\"value\":") + (on ? "true" : "false") +
                ",\"channel\":" + std::to_string(channel) + channel_fields(channel) + ",\"public_confirmed\":" + (public_confirmed ? "true" : "false"));
}

void BaseViewModel::set_mqtt(bool on) {
    send_config(std::string("\"path\":\"mqtt.enabled\",\"value\":") + (on ? "true" : "false"));
}

void BaseViewModel::set_auto_start(bool on) {
    autostart_subject_.set(on);
    persist();
}

void BaseViewModel::load_auto_start(bool on) {
    autostart_subject_.set(on);   // applied at startup without re-persisting
}

bool BaseViewModel::auto_start() const {
    return autostart_subject_.value();
}

void BaseViewModel::apply_device(const std::string& node_id, const std::string& firmware,
                                 const std::string& model) {
    node_id_subject_.set(node_id.c_str());
    firmware_subject_.set(firmware.c_str());
    model_subject_.set(model.c_str());
}

void BaseViewModel::apply_config(const std::string& preset, int hop, bool position, bool mqtt, const std::string& request, int frequency_slot) {
    if (!config_request_.empty() && request != config_request_) return;
    config_request_.clear();
    if (!preset.empty()) modem_preset_subject_.set(preset.c_str());
    if (hop >= 0) hop_limit_subject_.set(hop);
    frequency_slot_subject_.set(frequency_slot);
    position_subject_.set(position);
    mqtt_subject_.set(mqtt);
    config_state_.set("confirmed");
}


int BaseViewModel::channel_for_thread(const std::string& key) const {
    for (const auto& ch : channels_) if (!ch.identity.empty() && key == "ch:" + ch.identity) return ch.index;
    return -1;
}
std::string BaseViewModel::channel_fields(int index) const {
    const auto* ch = channel(index);
    return ",\"channel_id\":\"" + json::escape(ch ? ch->identity : "") + "\"";
}
void BaseViewModel::apply_health(const std::string& mode, const std::string& detail) {
    radio_mode_ = mode; radio_detail_ = detail;
}
bool BaseViewModel::service_request(const std::string& intent, const std::string& fields) {
    service_request_id_ = session_id_ + "service" + std::to_string(++cid_counter_);
    return send_intent("{\"intent\":\"" + json::escape(intent) + "\",\"id\":\"" + service_request_id_ + "\"" + fields + "}");
}
void BaseViewModel::apply_service(const json::Value& event) {
    if (event.str_at("id") != service_request_id_) return;
    service_event_ = event;
    services_version_.set(services_version_.value() + 1);
}
void BaseViewModel::apply_setup(const json::Value& event) {
    setup_ = event;
    if (event.str_at("id").empty()) services_version_.set(services_version_.value() + 1);
    else apply_service(event);
}
void BaseViewModel::apply_threads(const json::Value& event) {
    const auto local = drafts_;
    drafts_.clear(); pinned_.clear();
    if (const auto* states = event.find("threads")) for (const auto& [key, value] : states->obj) {
        drafts_[key] = value.str_at("draft");
        if (const auto* pin = value.find("pinned"); pin && pin->as_bool()) pinned_.insert(key);
    }
    if (const auto* v = event.find("history_enabled")) history_enabled_ = v->as_bool();
    for (const auto& key : dirty_drafts_) { const auto it = local.find(key); if (it != local.end()) drafts_[key] = it->second; }
    messages_version_subject_.set(messages_version_subject_.value() + 1);
}
std::string BaseViewModel::draft(const std::string& key) const {
    const auto it = drafts_.find(key); return it == drafts_.end() ? "" : it->second;
}
bool BaseViewModel::pinned(const std::string& key) const { return pinned_.count(key); }
void BaseViewModel::send_thread_state(const std::string& key) {
    send_intent("{\"intent\":\"thread.state\",\"thread\":\"" + json::escape(key) +
        "\",\"state\":{\"draft\":\"" + json::escape(draft(key)) + "\",\"pinned\":" + (pinned(key) ? "true" : "false") + "}}");
}
void BaseViewModel::save_draft(const std::string& key, const std::string& text) {
    if (text.size() > 200 || drafts_[key] == text) return;
    drafts_[key] = text;
    dirty_drafts_.insert(key);
    draft_deadline_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + 650;
}
void BaseViewModel::flush_drafts() {
    auto pending = dirty_drafts_;
    dirty_drafts_.clear();
    for (const auto& key : pending) send_thread_state(key);
}
void BaseViewModel::toggle_pin(const std::string& key) {
    if (pinned(key)) pinned_.erase(key); else pinned_.insert(key);
    send_thread_state(key);
    messages_version_subject_.set(messages_version_subject_.value() + 1);
}
void BaseViewModel::set_quiet_hours(int start, int end) {
    if (start < -1 || start > 1439 || end < -1 || end > 1439) return;
    quiet_start_ = start; quiet_end_ = end; persist();
}
bool BaseViewModel::quiet_now() const {
    if (quiet_start_ < 0 || quiet_end_ < 0) return false;
    const auto now = std::time(nullptr); std::tm local{};
    if (!localtime_r(&now, &local)) return false;
    const int minute = local.tm_hour * 60 + local.tm_min;
    return quiet_start_ <= quiet_end_ ? minute >= quiet_start_ && minute < quiet_end_ : minute >= quiet_start_ || minute < quiet_end_;
}

} // namespace viewmodel
