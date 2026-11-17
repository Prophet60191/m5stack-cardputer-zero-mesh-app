/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "json.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <random>
#include <utility>
#include <unordered_map>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

BaseViewModel::BaseViewModel()
    : dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false) {
    // A per-session prefix so our client ids never collide with those from an
    // earlier UI session (the supervisor keeps history across UI reconnects).
    std::random_device rd;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "c%08x%08x%08x%08x-", rd(), rd(), rd(), rd());
    session_id_ = buf;
}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
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

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::publish_all() {
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
    // A request accepted by the old transport may or may not have committed.
    // Keep its local edit protected; only current-connection results can finish it.
    thread_saves_.clear();
    blocked_saves_.clear();
    if (!connected && !service_request_id_.empty()) {
        json::Value error;
        json::parse("{\"event\":\"error\",\"id\":\"" + service_request_id_ + "\",\"text\":\"Connection lost; action result unknown. Check settings before retrying.\"}", error);
        apply_service(error);
    }
    if (!connected) import_request_id_.clear();
    connected_subject_.set(connected);
    if (connected) send_intent("{\"intent\":\"message.retention\",\"cap\":" + std::to_string(retention_cap_) + ",\"silent\":true}");
    if (!connected) {
        apply_health("unknown", radio_detail_);
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
        if (!std::isfinite(n.lat) || !std::isfinite(n.lon) || n.lat < -90 || n.lat > 90 || n.lon < -180 ||
            n.lon > 180) {
            n.hasPos = false;
            n.lat = n.lon = 0;
        }
        n.favorite = favorites_.count(n.id) != 0;
    }
    if (nodes_ == nodes)
        return;
    bool names_changed = nodes_.size() != nodes.size();
    for (size_t i = 0; !names_changed && i < nodes.size(); ++i)
        names_changed = std::tie(nodes_[i].id, nodes_[i].name, nodes_[i].isSelf) !=
                        std::tie(nodes[i].id, nodes[i].name, nodes[i].isSelf);
    nodes_ = std::move(nodes);
    nodes_version_subject_.set(nodes_version_subject_.value() + 1);
    if (names_changed)
        node_names_version_.set(node_names_version_.value() + 1);
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
    trim_all_threads();
    prune_unread();
    messages_version_subject_.set(messages_version_subject_.value() + 1);
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
    trim_thread(key);
    prune_unread();
    messages_version_subject_.set(messages_version_subject_.value() + 1);
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
        if (it->first == key || it->first.rfind(key + "@", 0) == 0) {
            dirty_drafts_.erase(it->first);
            thread_saves_.erase(it->first);
            blocked_saves_.erase(it->first);
            it = drafts_.erase(it);
        } else
            ++it;
    }
    pinned_.erase(key);
    send_intent("{\"intent\":\"thread.delete\",\"thread\":\"" + json::escape(key) + "\"}");
    muted_.erase(key);   // forget everything local about the conversation
    persist();
    messages_version_subject_.set(messages_version_subject_.value() + 1);
}

void BaseViewModel::delete_message(const std::string& id) {
    if (id.empty()) return;
    const auto before = messages_.size();
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(),
                                   [&](const MessageInfo& m) { return m.id == id; }),
                    messages_.end());
    send_intent("{\"intent\":\"message.delete\",\"message_id\":\"" + json::escape(id) + "\"}");
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
    if (trim_all_threads()) {
        prune_unread();
        messages_version_subject_.set(messages_version_subject_.value() + 1);
    }
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

bool BaseViewModel::trim_thread(const std::string &key) {
    if (retention_cap_ <= 0)
        return false;
    int total = 0;
    for (const auto& m : messages_) if (thread_key_of(m) == key) ++total;
    int remove = total - retention_cap_;
    if (remove <= 0)
        return false;
    std::vector<MessageInfo> kept;
    kept.reserve(messages_.size());
    int skipped = 0;
    for (auto& m : messages_) {
        if (thread_key_of(m) == key && skipped < remove) { ++skipped; continue; }
        kept.push_back(std::move(m));
    }
    messages_.swap(kept);
    return true;
}

bool BaseViewModel::trim_all_threads() {
    if (retention_cap_ <= 0)
        return false;
    std::unordered_map<std::string, int> counts;
    std::vector<bool> keep(messages_.size());
    size_t count = 0;
    for (size_t i = messages_.size(); i-- > 0;) {
        if (++counts[thread_key_of(messages_[i])] <= retention_cap_) {
            keep[i] = true;
            ++count;
        }
    }
    if (count == messages_.size())
        return false;
    std::vector<MessageInfo> kept;
    kept.reserve(count);
    for (size_t i = 0; i < messages_.size(); ++i)
        if (keep[i])
            kept.push_back(std::move(messages_[i]));
    messages_.swap(kept);
    return true;
}

void BaseViewModel::set_dm_target(const std::string& id, const std::string& name) {
    dm_target_id_ = id;
    dm_target_name_ = name;
}

void BaseViewModel::clear_dm_target() {
    dm_target_id_.clear();
    dm_target_name_.clear();
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

void BaseViewModel::tick(std::chrono::steady_clock::time_point at, long wall) {
    if (presentation_time_ != wall) {
        presentation_time_ = wall;
        clock_version_.set(clock_version_.value() + 1);
    }
    const auto now = at.time_since_epoch();
    if (!dirty_drafts_.empty() &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count() >= draft_deadline_)
        flush_drafts_at(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    if (!config_request_.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now).count() > config_deadline_) {
        config_unknown();
        apply_notice("Radio confirmation timed out; settings unknown");
    }
    if (!service_request_id_.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now).count() >= service_deadline_) {
        cancel_import();
        json::Value error;
        json::parse("{\"event\":\"error\",\"id\":\"" + service_request_id_ + "\",\"text\":\"Action timed out; result unknown. Check settings before retrying.\"}", error);
        apply_service(error);
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

void BaseViewModel::set_gps_enabled(bool on) {
    send_config(std::string("\"path\":\"position.gps_mode\",\"value\":\"") + (on ? "ENABLED" : "DISABLED") + "\"");
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

void BaseViewModel::apply_config(const std::string& preset, int hop, bool position, bool mqtt, const std::string& request, int frequency_slot, const std::string& gps_mode) {
    if (!config_request_.empty() && request != config_request_) return;
    config_request_.clear();
    if (!preset.empty()) modem_preset_subject_.set(preset.c_str());
    if (hop >= 0) hop_limit_subject_.set(hop);
    frequency_slot_subject_.set(frequency_slot);
    position_subject_.set(position);
    gps_mode_subject_.set(gps_mode.c_str());
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
    if (radio_mode_ == mode && radio_detail_ == detail)
        return;
    radio_mode_ = mode; radio_detail_ = detail;
    health_version_.set(health_version_.value() + 1);
}
bool BaseViewModel::service_request(const std::string& intent, const std::string& fields) {
    if (intent == "channel.scan" || intent == "channel.preview") cancel_import();
    service_request_id_ = session_id_ + "service" + std::to_string(++cid_counter_);
    service_intent_ = intent;
    if (intent == "channel.scan" || intent == "channel.preview") import_request_id_ = service_request_id_;
    service_deadline_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
        + (intent == "channel.add" ? 90000 : 60000);
    service_event_ = {}; // An unsolicited setup update must not reuse an old result.
    if (send_intent("{\"intent\":\"" + json::escape(intent) + "\",\"id\":\"" + service_request_id_ + "\"" + fields + "}")) return true;
    service_request_id_.clear(); service_intent_.clear(); service_deadline_ = 0; import_request_id_.clear();
    return false;
}
void BaseViewModel::cancel_import() {
    if (import_request_id_.empty()) return;
    send_intent("{\"intent\":\"channel.import.cancel\",\"request\":\"" + import_request_id_ + "\"}");
    import_request_id_.clear();
}
void BaseViewModel::cancel_service_request() {
    cancel_import();
    service_request_id_.clear(); service_intent_.clear(); service_deadline_ = 0;
}
void BaseViewModel::apply_service(const json::Value& event) {
    if (service_request_id_.empty()) return;
    const bool validation_error = event.str_at("event") == "error" && event.str_at("id").empty() &&
        !service_intent_.empty() && event.str_at("intent") == service_intent_;
    if (event.str_at("id") != service_request_id_ && !validation_error) return;
    service_event_ = event;
    if (validation_error) {
        json::Value ident; ident.type = json::Value::Str; ident.str = service_request_id_;
        service_event_.obj["id"] = ident;
    }
    if (event.str_at("event") == "error") cancel_import();
    if (service_intent_ == "channel.add" ||
        (event.str_at("event") == "channel.preview" && event.int_at("new") == 0)) import_request_id_.clear();
    service_request_id_.clear(); service_intent_.clear(); service_deadline_ = 0;
    services_version_.set(services_version_.value() + 1);
}
void BaseViewModel::apply_setup(const json::Value& event) {
    setup_ = event;
    if (event.str_at("id").empty()) services_version_.set(services_version_.value() + 1);
    else apply_service(event);
}
std::vector<ConversationRow> BaseViewModel::conversation_rows(const std::string& search) const {
    struct Item { ConversationRow row; bool pinned = false; bool match = false; };
    std::vector<Item> items;
    items.reserve(messages_.size() + drafts_.size());
    std::unordered_map<std::string,size_t> index;
    index.reserve(messages_.size() + drafts_.size());
    std::unordered_map<std::string,std::string> peers, groups;
    for (const auto& node : nodes_) peers.emplace(node.id, node.name.empty() ? node.id : node.name);
    for (const auto& channel : channels_) if (!channel.identity.empty())
        groups.emplace("ch:" + channel.identity, "# " + channel_label(channel.index));
    const auto peer_name = [&](const std::string& id) {
        const auto found = peers.find(id); return found == peers.end() ? id : found->second;
    };
    const auto add = [&](const std::string& key, const std::string& peer, const std::string& name,
                         const std::string& preview, long time) {
        index.emplace(key, items.size());
        items.push_back({{key, peer, name, preview, time}, pinned(key), false});
        return items.size()-1;
    };
    for (const auto& message : messages_) {
        const auto key = thread_key_of(message);
        auto found = index.find(key);
        size_t slot;
        if (found == index.end()) {
            const bool group = is_group_thread(key);
            const std::string peer = group ? "" : (message.outgoing ? message.to : message.from);
            const auto live = groups.find(key);
            const std::string name = !group ? peer_name(peer) : live != groups.end() ? live->second :
                "Archived: " + (message.channelName.empty() ? std::string("Default") : message.channelName) +
                    (message.channelPrivate ? " [Private]" : " [Public]");
            slot = add(key, peer, name, message.text, message.time);
        } else slot = found->second;
        auto& item = items[slot];
        if (message.time >= item.row.last_time) {
            item.row.last_time = message.time; item.row.preview = message.text;
        }
        if (!search.empty() && message.text.find(search) != std::string::npos) item.match = true;
    }
    // The first nonempty draft in map order has the same precedence as
    // conversation_draft(), including channel-qualified DM drafts.
    std::unordered_map<std::string,std::string> drafts;
    for (const auto& [key,text] : drafts_) if (!text.empty()) {
        drafts.emplace(key.substr(0,key.find('@')),text);
        drafts.emplace(key,text);
    }
    for (const auto& key : conversation_keys()) {
        const auto draft = drafts.find(key);
        auto found = index.find(key);
        const bool group = is_group_thread(key);
        size_t slot;
        if (found == index.end()) {
            const auto live = groups.find(key);
            const std::string peer = group ? "" : key.substr(3);
            slot = add(key, peer, group ? (live == groups.end() ? "Archived channel" : live->second) : peer_name(peer), "No messages", 0);
        } else slot = found->second;
        if (draft != drafts.end()) items[slot].row.preview = "Draft: " + draft->second;
    }
    if (!search.empty()) items.erase(std::remove_if(items.begin(),items.end(),[&](const Item& item) {
        return !item.match && item.row.name.find(search) == std::string::npos && item.row.preview.find(search) == std::string::npos;
    }),items.end());
    std::stable_sort(items.begin(),items.end(),[](const Item& a,const Item& b) {
        if (a.pinned != b.pinned) return a.pinned;
        return a.row.last_time > b.row.last_time;
    });
    std::vector<ConversationRow> rows;
    rows.reserve(items.size());
    for (auto& item : items) {
        if (item.pinned) item.row.name = "* " + item.row.name;
        rows.push_back(std::move(item.row));
    }
    return rows;
}

void BaseViewModel::apply_threads(const json::Value& event) {
    std::map<std::string,std::string> drafts;
    std::set<std::string> pins;
    bool history_enabled = history_enabled_;
    if (const auto* states = event.find("threads")) for (const auto& [key, value] : states->obj) {
        if (drafts.size() >= 256) break;
        if (key.size() <= 3 || key.size() > 160 || value.type != json::Value::Obj ||
            (key.rfind("ch:",0) != 0 && key.rfind("dm:",0) != 0) || value.str_at("draft").size() > 200) continue;
        if (key.rfind("dm:",0) == 0) {
            const auto peer = key.substr(3,key.find('@') == std::string::npos ? std::string::npos : key.find('@')-3);
            if (peer.size() != 9 || peer[0] != '!' || !std::all_of(peer.begin()+1,peer.end(),[](unsigned char c) { return std::isxdigit(c); })) continue;
        }
        drafts[key] = value.str_at("draft");
        if (const auto* pin = value.find("pinned"); pin && pin->as_bool()) pins.insert(key);
    }
    if (const auto* v = event.find("history_enabled")) history_enabled = v->as_bool();
    for (const auto& key : dirty_drafts_) {
        const auto it = drafts_.find(key);
        if (it != drafts_.end()) drafts[key] = it->second;
        if (pinned(key))
            pins.insert(key);
        else
            pins.erase(key);
    }
    if (drafts != drafts_ || pins != pinned_ || history_enabled != history_enabled_) {
        drafts_ = std::move(drafts); pinned_ = std::move(pins); history_enabled_ = history_enabled;
        messages_version_subject_.set(messages_version_subject_.value() + 1);
    }
    // An unchanged payload can still complete a correlated preference request.
    if (!event.str_at("id").empty()) apply_service(event);
}
std::string BaseViewModel::draft(const std::string& key) const {
    const auto it = drafts_.find(key); return it == drafts_.end() ? "" : it->second;
}
bool BaseViewModel::pinned(const std::string& key) const { return pinned_.count(key); }
void BaseViewModel::remember_conversation(const std::string& key) {
    if (drafts_.count(key)) return;
    if (drafts_.size() >= 256) {
        apply_notice("Conversation state limit reached");
        return;
    }
    drafts_[key] = "";
    dirty_drafts_.insert(key);
    flush_drafts();
}
std::vector<std::string> BaseViewModel::conversation_keys() const {
    std::set<std::string> keys = pinned_;
    for (const auto& item : drafts_) keys.insert(item.first.substr(0, item.first.find('@')));
    return {keys.begin(), keys.end()};
}
std::string BaseViewModel::conversation_draft(const std::string& key) const {
    for (const auto& item : drafts_) if (!item.second.empty() &&
        (item.first == key || item.first.rfind(key + "@", 0) == 0)) return item.second;
    return "";
}
bool BaseViewModel::send_thread_state(const std::string &key, long long now) {
    if (blocked_saves_.count(key))
        return true;
    const auto pending = thread_saves_.find(key);
    if (pending != thread_saves_.end() && pending->second.deadline > now)
        return true;
    ThreadSave save{session_id_ + "thread" + std::to_string(++cid_counter_), draft(key), pinned(key),
                    now + 2000};
    thread_saves_[key] = save;
    const std::string line = "{\"intent\":\"thread.state\",\"id\":\"" + save.request + "\",\"thread\":\"" +
                             json::escape(key) + "\",\"state\":{\"draft\":\"" + json::escape(save.draft) +
                             "\",\"pinned\":" + (save.pinned ? "true" : "false") + "}}";
    if (!command_sink_ || !command_sink_(line)) {
        thread_saves_.erase(key);
        return false;
    }
    return true;
}
void BaseViewModel::apply_thread_result(const json::Value &event) {
    const std::string request = event.str_at("id");
    for (auto it = thread_saves_.begin(); it != thread_saves_.end(); ++it) {
        if (request.empty() || it->second.request != request)
            continue;
        if (event.str_at("event") == "error") {
            blocked_saves_.insert(it->first);
            thread_saves_.erase(it);
            return;
        }
        const auto *durable = event.find("durable");
        if (event.str_at("event") != "thread.saved" || event.str_at("thread") != it->first || !durable ||
            durable->type != json::Value::Bool)
            return;
        if (draft(it->first) == it->second.draft && pinned(it->first) == it->second.pinned)
            dirty_drafts_.erase(it->first);
        thread_saves_.erase(it);
        return;
    }
}
void BaseViewModel::save_draft(const std::string& key, const std::string& text) {
    if (text.size() > 200 || draft(key) == text)
        return;
    if (!drafts_.count(key) && drafts_.size() >= 256) {
        apply_notice("Conversation state limit reached");
        return;
    }
    drafts_[key] = text;
    thread_saves_.erase(key);
    blocked_saves_.erase(key);
    dirty_drafts_.insert(key);
    draft_deadline_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + 650;
}
void BaseViewModel::flush_drafts() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    flush_drafts_at(now);
}
void BaseViewModel::flush_drafts_at(long long now) {
    draft_deadline_ = now + 1000;
    if (!connected_subject_.value())
        return;
    bool rejected = false;
    for (const auto &key : dirty_drafts_)
        if (!send_thread_state(key, now))
            rejected = true;
    if (rejected && !draft_send_failed_)
        apply_notice("Draft is unsaved; waiting for the supervisor");
    draft_send_failed_ = rejected;
}
void BaseViewModel::toggle_pin(const std::string& key) {
    if (!drafts_.count(key) && drafts_.size() >= 256) {
        apply_notice("Conversation state limit reached");
        return;
    }
    drafts_.try_emplace(key, "");
    if (pinned(key)) pinned_.erase(key); else pinned_.insert(key);
    dirty_drafts_.insert(key);
    thread_saves_.erase(key);
    blocked_saves_.erase(key);
    flush_drafts();
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
