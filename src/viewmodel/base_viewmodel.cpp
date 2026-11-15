/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "json.h"

#include <algorithm>
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
    char buf[16];
    std::snprintf(buf, sizeof(buf), "c%06x-", static_cast<unsigned>(rd() & 0xFFFFFFu));
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
    if (!connected) {
        radio_state_subject_.set("off");
    }
}

void BaseViewModel::apply_status(const std::string& radio, const std::string& region,
                                 const std::string& owner) {
    radio_state_subject_.set(radio.c_str());
    region_subject_.set(region.c_str());
    owner_subject_.set(owner.c_str());
}

void BaseViewModel::set_nodes(std::vector<NodeInfo> nodes) {
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
    channels_ = std::move(channels);
    channels_version_subject_.set(channels_version_subject_.value() + 1);
}

void BaseViewModel::set_messages(std::vector<MessageInfo> messages) {
    messages_ = std::move(messages);
    messages_version_subject_.set(messages_version_subject_.value() + 1);
    trim_all_threads();
}

void BaseViewModel::add_message(MessageInfo message) {
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
    if (incoming && !is_muted(key) && !mute_all_) {
        unread_[key] += 1;
        unread_version_subject_.set(unread_version_subject_.value() + 1);
        incoming_message_subject_.set(incoming_message_subject_.value() + 1);
    }

    messages_.push_back(std::move(message));
    messages_version_subject_.set(messages_version_subject_.value() + 1);
    trim_thread(key);   // honour the per-thread retention cap
}

// --- user actions -> intents ------------------------------------------------
void BaseViewModel::set_command_sink(std::function<void(const std::string&)> sink) {
    command_sink_ = std::move(sink);
}

void BaseViewModel::send_intent(const std::string& line) {
    if (command_sink_) {
        command_sink_(line);
    }
}

void BaseViewModel::radio_start() { send_intent("{\"intent\":\"radio.start\"}"); }
void BaseViewModel::radio_stop() { send_intent("{\"intent\":\"radio.stop\"}"); }

void BaseViewModel::emit_message_send(int channel, const std::string& text,
                                      const std::string& dest, const std::string& cid) {
    std::string line = "{\"intent\":\"message.send\",\"channel\":" + std::to_string(channel) +
                       ",\"text\":\"" + json::escape(text) + "\",\"cid\":\"" + cid + "\"" +
                       ",\"wantAck\":" + (want_ack_ ? "true" : "false");
    if (!dest.empty()) {
        line += ",\"dest\":\"" + json::escape(dest) + "\"";
    }
    line += "}";
    send_intent(line);
}

void BaseViewModel::send_message(int channel, const std::string& text, const std::string& dest) {
    if (text.empty()) {
        return;
    }
    const std::string cid = session_id_ + std::to_string(++cid_counter_);
    emit_message_send(channel, text, dest, cid);

    // Local echo (the supervisor does not echo our own sends; delivery arrives
    // later as a "delivery" event keyed by this cid).
    MessageInfo m;
    m.from = "self";
    m.fromName = "me";
    m.to = dest.empty() ? "broadcast" : dest;
    m.text = text;
    m.id = cid;
    m.channel = channel;
    m.outgoing = true;
    m.status = want_ack_ ? kMsgSending : 0;   // no ack requested -> no pending state
    m.time = static_cast<long>(std::time(nullptr));
    add_message(std::move(m));
}

void BaseViewModel::set_delivery(const std::string& id, int status, int hops) {
    if (id.empty()) return;
    bool changed = false;
    for (auto& m : messages_) {
        if (m.outgoing && m.id == id) {
            m.status = status;
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
    muted_.erase(key);   // forget everything local about the conversation
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
            m.status = kMsgSending;
            const std::string dest = (m.to == "broadcast" || m.to.empty()) ? std::string() : m.to;
            emit_message_send(m.channel, m.text, dest, m.id);   // same cid -> updates this bubble
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
    if (cap < 0) cap = 0;
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

void BaseViewModel::send_position(int channel, const std::string& dest) {
    std::string line = "{\"intent\":\"position.send\",\"channel\":" + std::to_string(channel);
    if (!dest.empty()) line += ",\"dest\":\"" + json::escape(dest) + "\"";
    line += "}";
    send_intent(line);
}

void BaseViewModel::request_position(const std::string& node_id) {
    if (node_id.empty()) return;
    send_intent("{\"intent\":\"position.request\",\"node\":\"" + json::escape(node_id) + "\"}");
}

void BaseViewModel::traceroute(const std::string& node_id) {
    if (node_id.empty()) return;
    send_intent("{\"intent\":\"traceroute\",\"node\":\"" + json::escape(node_id) + "\"}");
}

void BaseViewModel::request_channel_share(const std::string& name) {
    send_intent("{\"intent\":\"channel.share\",\"name\":\"" + json::escape(name) + "\"}");
}

void BaseViewModel::apply_traceroute(bool ok, const std::string& text) {
    traceroute_result_ = ok ? text : (text.empty() ? "No route found." : text);
    traceroute_subject_.set(traceroute_subject_.value() + 1);
}

void BaseViewModel::apply_channel_share(const std::string& url) {
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
    retention_cap_ = retention < 0 ? 0 : retention;
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

void BaseViewModel::set_modem_preset(const std::string& preset) {
    modem_preset_subject_.set(preset.c_str());
    send_intent("{\"intent\":\"config.set\",\"path\":\"lora.modem_preset\",\"value\":\"" +
                json::escape(preset) + "\"}");
}

void BaseViewModel::set_hop_limit(int hops) {
    hop_limit_subject_.set(hops);
    send_intent("{\"intent\":\"config.set\",\"path\":\"lora.hop_limit\",\"value\":" +
                std::to_string(hops) + "}");
}

void BaseViewModel::set_position_sharing(bool on) {
    position_subject_.set(on);
    send_intent(std::string("{\"intent\":\"config.set\",\"path\":\"position.enabled\",\"value\":") +
                (on ? "true" : "false") + "}");
}

void BaseViewModel::set_mqtt(bool on) {
    mqtt_subject_.set(on);
    send_intent(std::string("{\"intent\":\"config.set\",\"path\":\"mqtt.enabled\",\"value\":") +
                (on ? "true" : "false") + "}");
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

void BaseViewModel::apply_config(const std::string& preset, int hop, bool position, bool mqtt) {
    if (!preset.empty()) modem_preset_subject_.set(preset.c_str());
    if (hop > 0) hop_limit_subject_.set(hop);
    position_subject_.set(position);
    mqtt_subject_.set(mqtt);
}

} // namespace viewmodel
