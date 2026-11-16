/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"

#include <string>
#include <vector>

namespace screen {

// Phone-style messaging: a conversations list (configured channels + one thread
// per person you DM, plus "New message"), and a thread view you open into.
class MessagesScreen : public BaseScreen {
public:
    MessagesScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~MessagesScreen() override;

    void on_up() override;
    void on_down() override;
    void on_enter() override;
    bool on_back() override;
    bool on_key(uint32_t key) override;
    bool is_composing() override;

private:
    enum class Mode { List, Thread };
    enum class MenuKind { None, Thread, Message, Canned, Channel, PublicLocation };

    void build_content(lv_obj_t* content) override;
    void rebuild();
    void rebuild_list();
    void rebuild_thread();
    void open_thread(const std::string& key, const std::string& peer_id, const std::string& peer_name);
    void open_picker();
    void close_picker();
    void rebuild_picker();
    void start_compose();
    void commit_compose();
    void cancel_compose();
    void update_hint();
    int send_channel();
    std::string draft_key() const;
    static void draft_changed(lv_event_t* event);
    std::string peer_name_for(const std::string& id);
    std::string channel_name(int index);          // display name for a channel index
    std::string group_name(const std::string& key);  // "# <channel>" for a group thread
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);

    // Action menus (reuse the one popup-list primitive) + read-only overlay.
    void open_thread_menu();
    void open_message_menu(const std::string& msg_id);
    void open_canned_picker();
    void open_channel_picker();
    void open_public_location_confirmation();
    void create_menu();
    void rebuild_menu();
    void close_menu();
    void menu_activate();
    void open_info(const std::string& title, const std::string& body);
    void open_qr(const std::string& title, const std::string& url);
    void close_info();
    const viewmodel::MessageInfo* message_by_id(const std::string& id);
    std::string details_text(const viewmodel::MessageInfo& m);
    static void traceroute_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void channel_share_cb(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* body_ = nullptr;     // list or thread renders here
    lv_obj_t* context_ = nullptr;  // fixed channel/peer header, outside scrolling messages
    lv_obj_t* bar_ = nullptr;      // bottom compose bar
    lv_obj_t* hint_ = nullptr;
    lv_obj_t* input_ = nullptr;
    lv_obj_t* picker_ = nullptr;   // node picker overlay (nullptr when closed)
    lv_obj_t* menu_ = nullptr;     // action-menu overlay (nullptr when closed)
    lv_obj_t* overlay_ = nullptr;  // read-only info/details overlay

    Mode mode_ = Mode::List;
    std::string thread_key_;       // "ch" or "dm:<id>"
    std::string peer_id_;
    std::string peer_name_;
    int selected_ = 0;             // selection in the list / picker
    int msg_sel_ = -1;             // thread: -1 = compose row, 0..n-1 = a bubble
    bool composing_ = false;
    bool searching_ = false;
    std::string search_;
    std::string dm_channel_id_;
    std::vector<std::string> channel_picker_ids_;
    bool at_bottom_ = true;        // smart auto-scroll
    int dm_channel_ = -1;          // explicitly chosen; never a primary fallback
    int location_channel_ = -1;
    std::string location_peer_;

    MenuKind menu_kind_ = MenuKind::None;
    bool awaiting_traceroute_ = false;   // a traceroute result overlay is expected
    bool awaiting_share_ = false;        // a channel-share QR overlay is expected
    bool share_visible_ = false;
    std::string menu_target_;      // thread key or message id the menu acts on
    int menu_sel_ = 0;
    std::vector<std::string> menu_labels_;
    std::vector<int> menu_actions_;

    std::vector<std::string> conv_key_, conv_peer_, conv_name_;   // conversations list
    std::vector<std::string> picker_id_, picker_name_;            // node picker
    std::vector<lv_obj_t*> thread_rows_;                          // bubble rows (for highlight/scroll)
    std::vector<std::string> thread_msg_ids_;                     // parallel message ids
};

} // namespace screen
