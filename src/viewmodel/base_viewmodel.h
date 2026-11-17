/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"
#include "mesh_data.h"
#include "json.h"

#include "lvgl.h"

#include <functional>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void show_messages_page();
    void show_nodes_page();
    void show_map_page();
    void show_settings_page();
    void show_menu_page();
    void show_next_page();
    void show_prev_page();

    // The home carousel: Left/Right roll the selection, Enter opens it.
    lv_subject_t* menu_index_subject();   // 0 .. kMenuEntryCount-1
    void menu_prev();
    void menu_next();
    void open_menu_selection();
    void request_quit();

    // --- mesh state (bound by the Messages / Nodes / Settings screens) -------
    lv_subject_t* radio_state_subject();      // "off" | "starting" | "online" | "error"
    lv_subject_t* region_subject();
    lv_subject_t* owner_subject();
    lv_subject_t* connected_subject();        // supervisor socket connected?
    lv_subject_t* nodes_version_subject();    // bumped whenever the list changes
    lv_subject_t* channels_version_subject();
    lv_subject_t* messages_version_subject();
    lv_subject_t* unread_version_subject();    // bumped when unread counts change
    lv_subject_t* incoming_message_subject();  // bumped on each incoming message

    const std::vector<NodeInfo>& nodes() const { return nodes_; }
    const std::vector<ChannelInfo>& channels() const { return channels_; }
    const std::vector<MessageInfo>& messages() const { return messages_; }

    // Applied on the UI thread as supervisor events arrive.
    void set_connected(bool connected);
    void apply_status(const std::string& radio, const std::string& region, const std::string& owner);
    void set_nodes(std::vector<NodeInfo> nodes);
    lv_subject_t *node_names_subject() { return node_names_version_.native(); }
    void set_channels(std::vector<ChannelInfo> channels);
    void set_messages(std::vector<MessageInfo> messages);
    void add_message(MessageInfo message);
    void set_delivery(const std::string& id, int status, int hops = -1, const std::string& reason = "");
    int channel_for_thread(const std::string& key) const;
    std::string channel_fields(int index) const;
    void apply_health(const std::string& mode, const std::string& detail);
    lv_subject_t *health_subject() { return health_version_.native(); }
    lv_subject_t *clock_subject() { return clock_version_.native(); }
    long presentation_time() const { return presentation_time_; }
    const std::string& radio_mode() const { return radio_mode_; }
    const std::string& radio_detail() const { return radio_detail_; }
    lv_subject_t* services_subject() { return services_version_.native(); }
    bool service_request(const std::string& intent, const std::string& fields = "");
    void cancel_service_request();
    void cancel_import();
    void apply_service(const json::Value& event);
    const json::Value& service_event() const { return service_event_; }
    const json::Value& setup() const { return setup_; }
    void apply_setup(const json::Value& event);
    void apply_threads(const json::Value& event);
    void apply_thread_result(const json::Value &event);
    std::string draft(const std::string& key) const;
    void save_draft(const std::string& key, const std::string& text);
    void flush_drafts();
    bool thread_state_pending() const { return !dirty_drafts_.empty(); }
    bool pinned(const std::string& key) const;
    void remember_conversation(const std::string& key);
    std::vector<ConversationRow> conversation_rows(const std::string& search = "") const;
    std::vector<std::string> conversation_keys() const;
    std::string conversation_draft(const std::string& key) const;
    void toggle_pin(const std::string& key);
    bool history_enabled() const { return history_enabled_; }
    void set_quiet_hours(int start, int end);
    void load_quiet_hours(int start, int end) { quiet_start_ = start; quiet_end_ = end; }
    int quiet_start() const { return quiet_start_; }
    int quiet_end() const { return quiet_end_; }
    bool quiet_now() const;

    // --- per-thread state (app-local) ----------------------------------------
    int unread_count(const std::string& key) const;
    int total_unread() const;
    void mark_thread_read(const std::string& key);
    bool is_muted(const std::string& key) const;
    void toggle_mute(const std::string& key);

    // Thread / message actions. clear = wipe this device's history for the
    // conversation; delete = also forget the thread entirely (local-only, both).
    void clear_thread(const std::string& key);
    void delete_thread(const std::string& key);
    void delete_message(const std::string& id);
    void resend_message(const std::string& id);

    // The most recent incoming message (for a notification toast).
    const std::string& last_incoming_from() const { return last_incoming_from_; }
    const std::string& last_incoming_text() const { return last_incoming_text_; }
    const std::string& last_incoming_thread() const { return last_incoming_thread_; }

    // Canned ("quick") messages: editable presets, sendable with one key.
    const std::vector<std::string>& canned_messages() const { return canned_; }
    void add_canned(const std::string& text);
    void update_canned(int index, const std::string& text);
    void remove_canned(int index);

    // Messaging preferences (persisted).
    bool show_signal() const { return show_signal_; }
    void set_show_signal(bool on);
    int retention_cap() const { return retention_cap_; }
    void set_retention_cap(int cap);
    bool mute_all() const { return mute_all_; }
    void set_mute_all(bool on);
    bool want_ack() const { return want_ack_; }
    void set_want_ack(bool on);
    const std::set<std::string>& muted_threads() const { return muted_; }

    // Mesh-native actions (new supervisor intents).
    void send_position(int channel, const std::string& dest = "", bool public_confirmed = false);
    void request_position(const std::string& node_id, int channel);
    void traceroute(const std::string& node_id, int channel);
    void request_channel_share(int channel);
    const ChannelInfo* channel(int index) const;
    std::string channel_label(int index) const;
    const std::string& channel_share_name() const { return channel_share_name_; }
    void cancel_channel_share() { share_request_.clear(); }
    const std::string& share_request_id() const { return share_request_; }
    bool share_channel_valid() const;

    // Results of those actions, surfaced to the Messages screen.
    lv_subject_t* traceroute_subject();       // bumped when a traceroute returns
    lv_subject_t* channel_share_subject();    // bumped when a share URL is ready
    const std::string& traceroute_result() const { return traceroute_result_; }
    const std::string& channel_share_url() const { return channel_share_url_; }
    void apply_traceroute(bool ok, const std::string& text, const std::string& node, const std::string& request);
    void cancel_traceroute() { trace_request_.clear(); }
    void apply_channel_share(int channel, const std::string& name, const std::string& url, const std::string& request, const std::string& identity = "");

    // A one-line notice/error from the supervisor, shown as a toast.
    lv_subject_t* notice_subject();
    const std::string& last_notice() const { return last_notice_; }
    void apply_notice(const std::string& text);

    // Persistence: the app installs a sink called whenever a persisted field
    // changes. The load_* setters apply saved state without re-triggering it.
    void set_persist_hook(std::function<void()> fn) { on_persist_ = std::move(fn); }
    void load_canned(std::vector<std::string> list);
    void load_muted(std::vector<std::string> keys);
    void load_messaging_prefs(bool show_signal, int retention, bool mute_all, bool want_ack);

    // App-local favourites (persisted by the app).
    bool is_favorite(const std::string& id) const;
    void toggle_favorite(const std::string& id);
    std::vector<std::string> favorites() const;
    void load_favorites(const std::vector<std::string>& ids);

    // User actions -> intents sent to the supervisor.
    void set_command_sink(std::function<bool(const std::string&)> sink);
    void radio_start();
    void radio_stop();
    bool send_message(int channel, const std::string& text, const std::string& dest = "");
    void set_region(const std::string& region);
    void set_owner(const std::string& name, const std::string& short_name);

    // Device settings (bound by the Settings screen).
    lv_subject_t* modem_preset_subject();
    lv_subject_t* hop_limit_subject();
    lv_subject_t* frequency_slot_subject() { return frequency_slot_subject_.native(); }
    lv_subject_t* position_subject();     // broadcast my position
    lv_subject_t* gps_mode_subject() { return gps_mode_subject_.native(); }
    void set_gps_enabled(bool on);
    lv_subject_t* mqtt_subject();         // internet/MQTT bridge
    lv_subject_t* autostart_subject();    // start the radio on launch (app-local)
    lv_subject_t* node_id_subject();
    lv_subject_t* firmware_subject();
    lv_subject_t* model_subject();

    void set_modem_preset(const std::string& preset);
    void set_hop_limit(int hops);
    void set_frequency_slot(int slot);
    void set_position_sharing(bool on, int channel = -1, bool public_confirmed = false);
    void set_mqtt(bool on);
    void set_auto_start(bool on);
    void load_auto_start(bool on);   // apply persisted auto-start without re-saving
    bool auto_start() const;
    void apply_device(const std::string& node_id, const std::string& firmware, const std::string& model);
    void apply_config(const std::string& preset, int hop, bool position, bool mqtt, const std::string& request = "", int frequency_slot = 0, const std::string& gps_mode = "UNKNOWN");
    lv_subject_t* config_state_subject() { return config_state_.native(); }
    const char* config_state() const { return config_state_.value(); }
    void config_unknown();
    void tick(std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now(),
              long wall = static_cast<long>(std::time(nullptr)));

    // Direct-message target: when set, the Messages screen sends to this node.
    void set_dm_target(const std::string& id, const std::string& name);
    void clear_dm_target();
    const std::string& dm_target_id() const { return dm_target_id_; }
    const std::string& dm_target_name() const { return dm_target_name_; }

private:
    std::string radio_mode_, radio_detail_;
    reactive::IntSubject health_version_{0}, clock_version_{0};
    long presentation_time_ = static_cast<long>(std::time(nullptr));
    reactive::IntSubject services_version_{0};
    json::Value service_event_, setup_;
    std::string service_request_id_, service_intent_, import_request_id_;
    long long service_deadline_ = 0;
    std::map<std::string, std::string> drafts_;
    std::map<std::string, std::string> sent_draft_keys_;
    std::set<std::string> dirty_drafts_;
    struct ThreadSave {
        std::string request, draft;
        bool pinned;
        long long deadline;
    };
    std::map<std::string, ThreadSave> thread_saves_;
    std::set<std::string> blocked_saves_;
    bool draft_send_failed_ = false;
    long long draft_deadline_ = 0;
    std::set<std::string> pinned_;
    bool history_enabled_ = true;
    int quiet_start_ = -1, quiet_end_ = -1;
    bool send_thread_state(const std::string &key, long long now);
    void flush_drafts_at(long long now);
    void publish_all();
    bool send_intent(const std::string& line);

    model::BaseModel model_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;

    reactive::StringSubject<16> radio_state_subject_{"off"};
    reactive::StringSubject<24> region_subject_{"UNSET"};
    reactive::StringSubject<40> owner_subject_{""};
    reactive::BoolSubject connected_subject_{false};
    reactive::IntSubject nodes_version_subject_{0};
    reactive::IntSubject node_names_version_{0};
    reactive::IntSubject channels_version_subject_{0};
    reactive::IntSubject messages_version_subject_{0};
    reactive::IntSubject unread_version_subject_{0};
    reactive::IntSubject incoming_message_subject_{0};
    reactive::IntSubject traceroute_subject_{0};
    reactive::IntSubject channel_share_subject_{0};
    reactive::IntSubject notice_subject_{0};
    reactive::IntSubject menu_index_subject_{0};
    reactive::StringSubject<20> modem_preset_subject_{"MEDIUM_FAST"};
    reactive::IntSubject hop_limit_subject_{3};
    reactive::IntSubject frequency_slot_subject_{0};
    reactive::BoolSubject position_subject_{false};
    reactive::StringSubject<20> gps_mode_subject_{"UNKNOWN"};
    reactive::StringSubject<16> config_state_{"unknown"};
    std::string config_request_;
    long long config_deadline_ = 0;
    void send_config(const std::string& fields);
    reactive::BoolSubject mqtt_subject_{false};
    reactive::BoolSubject autostart_subject_{false};
    reactive::StringSubject<16> node_id_subject_{""};
    reactive::StringSubject<24> firmware_subject_{""};
    reactive::StringSubject<24> model_subject_{""};

    std::vector<NodeInfo> nodes_;
    std::vector<ChannelInfo> channels_;
    std::vector<MessageInfo> messages_;
    std::set<std::string> favorites_;
    std::map<std::string, int> unread_;     // thread key -> unread count
    std::set<std::string> muted_;           // muted thread keys
    std::vector<std::string> canned_{       // quick messages (editable in Settings)
        "OK", "On my way", "Copy that", "Need help", "Stand by", "What's your position?"};
    bool show_signal_ = false;
    int retention_cap_ = 0;                 // 0 = keep all
    bool mute_all_ = false;
    bool want_ack_ = true;
    std::string traceroute_result_;
    std::string trace_request_, trace_node_;
    std::string channel_share_url_;
    std::string channel_share_name_;
    std::string share_request_;
    int share_channel_ = -1;
    std::string share_identity_;
    std::string last_notice_;
    std::function<void()> on_persist_;
    void persist();
    bool trim_thread(const std::string &key);
    bool trim_all_threads();
    void prune_unread();
    std::string last_incoming_from_;
    std::string last_incoming_text_;
    std::string last_incoming_thread_;
    std::string dm_target_id_;
    std::string dm_target_name_;
    std::string session_id_;          // per-session client-id prefix (collision-free)
    unsigned long cid_counter_ = 0;

    bool emit_message_send(int channel, const std::string& text,
                           const std::string& dest, const std::string& cid);

    void apply_favorites();

    std::function<bool(const std::string&)> command_sink_;
};

} // namespace viewmodel
