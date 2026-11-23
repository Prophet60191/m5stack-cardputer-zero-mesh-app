// SPDX-License-Identifier: MIT
// The unified channel form is local UI state; only the supervisor applies it.
#include "settings_screen.h"
#include <algorithm>
#include <charconv>
#include <cstdio>

namespace screen {
namespace {
std::string quote(const std::string &s) { return "\"" + json::escape(s) + "\""; }
std::string number(double value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.17g", value);
    return text;
}
const char *boolean(bool value) { return value ? "true" : "false"; }
bool flag(const json::Value &value, const std::string &key) {
    const auto *p = value.find(key);
    return p && p->as_bool();
}
const std::vector<std::string> regions = {"US", "EU_868", "EU_433", "ANZ", "NZ_865", "KR",
                                          "TW", "JP",     "SG_923", "TH",  "MY_919", "PH_915"};
const std::vector<std::string> presets = {"LONG_FAST",   "LONG_MODERATE", "LONG_SLOW", "MEDIUM_FAST",
                                          "MEDIUM_SLOW", "SHORT_FAST",    "SHORT_SLOW"};
int index_of(const std::vector<std::string> &items, const std::string &value) {
    auto it = std::find(items.begin(), items.end(), value);
    return it == items.end() ? 0 : static_cast<int>(it - items.begin());
}
enum Field {
    Method,
    Name,
    Region,
    Preset,
    Slot,
    Hop,
    Tx,
    Position,
    Mqtt,
    Destination,
    Profile,
    Suggested,
    Reload,
    Cancel
};
} // namespace

void SettingsScreen::channel_setup_start() {
    view_model().cancel_service_request();
    channel_draft_ = {};
    channel_flow_ = true;
    channel_field_ = Name;
    request_service("flow:context", "channel.setup.context");
}
void SettingsScreen::channel_setup_tuning(const json::Value &value) {
    channel_draft_.region = value.str_at("region");
    channel_draft_.preset = value.str_at("preset");
    channel_draft_.slot = value.int_at("slot");
    channel_draft_.hop = value.int_at("hop", 3);
    channel_draft_.use_preset = flag(value, "use_preset");
    channel_draft_.override_frequency =
        value.find("override_frequency") ? value.find("override_frequency")->num : 0;
    if (const auto *offset = value.find("frequency_offset"))
        channel_draft_.frequency_offset = offset->num;
}
void SettingsScreen::channel_setup_form() {
    close_overlay();
    auto &d = channel_draft_;
    std::vector<std::string> rows;
    channel_fields_order_.clear();
    const auto add = [&](int key, const std::string &text) {
        channel_fields_order_.push_back(key);
        rows.push_back(text);
    };
    if (!d.error.empty())
        add(-1, "Problem: " + d.error);
    add(-2, "Channel");
    if (d.source == "import") {
        std::string names;
        if (const auto *channels = d.preview.find("channels"))
            for (const auto &ch : channels->arr) {
                if (flag(ch, "existing"))
                    continue;
                if (!names.empty())
                    names += "; ";
                names += ch.str_at("name") + (flag(ch, "private") ? " [Private]" : " [Public]");
            }
        if (d.preview.int_at("new") > 1)
            names = std::to_string(d.preview.int_at("new")) + " channels";
        add(Name, names.empty() ? "Channel details: scan or paste first"
                                : names + " (details)\nKey: imported, hidden");
    } else {
        add(Name,
            "Channel name: " +
                (d.name.empty() ? (d.source == "private" ? "Required (1..11 bytes)" : "Default (from preset)")
                                : d.name) +
                (d.source == "private" ? "\nKey: new random secret, hidden" : "\nKey: standard public key"));
    }
    add(Method, "Add by: " + (d.source == "import" ? std::string("QR / share link")
                              : d.source.empty()   ? "Choose method"
                                                   : d.source));
    add(-2, "Radio connection - all channels");
    add(Region, "Region (whole radio): " + d.region);
    add(Preset, "Modem (whole radio): " + (d.use_preset ? d.preset : "Custom (preserved)"));
    add(Slot, d.override_frequency > 0
                  ? "Frequency: fixed " + std::to_string(d.override_frequency) + " MHz"
                  : "Frequency slot: " +
                        (d.slot == 0 ? std::string("Auto (primary name)") : std::to_string(d.slot)));
    if (d.frequency_offset)
        add(-1, "Calibration offset (preserved): " + std::to_string(d.frequency_offset));
    add(Hop, "Hop limit: " + std::to_string(d.hop));
    if (d.suggested.is_obj())
        add(Suggested, "Use radio settings from QR / link");
    add(Tx, std::string("Transmit mode: ") + (d.tx ? "TX/RX" : "Receive only"));
    add(-2, "Sharing - new channel");
    std::string location = "Off on new channel";
    if (d.position) {
        location = d.name.empty() ? "Default" : d.name;
        if (d.source == "import") {
            int ordinal = 0;
            if (const auto *channels = d.preview.find("channels"))
                for (const auto &ch : channels->arr) {
                    if (flag(ch, "existing"))
                        continue;
                    if (ordinal++ == d.position_target) {
                        location = ch.str_at("name") + (flag(ch, "private") ? " [Private]" : " [Public]");
                        break;
                    }
                }
        }
    }
    add(Position, "Automatic location: " + location);
    bool public_channel = d.source == "public";
    if (d.source == "import")
        if (const auto* channels = d.preview.find("channels"))
            for (const auto& ch : channels->arr)
                if (!flag(ch, "existing") && !flag(ch, "private")) public_channel = true;
    if (public_channel)
        add(-1, "Public traffic can be read or forwarded online by other radios.");
    add(Mqtt,
        std::string("Internet forwarding: ") + (d.mqtt ? "On (configured MQTT)" : "Off on new channel"));
    if (d.mqtt)
        add(-1, "Uses the configured broker; other forwarding channels can send too.");
    add(-2, "Save destination");
    add(Destination, std::string("Save to: ") + (d.profile_only ? "Separate profile only" : "Current setup"));
    add(Profile,
        "Profile name: " + (d.profile_name.empty() ? (d.profile_only ? std::string("Required") : "Optional")
                                                   : d.profile_name));
    if (!d.preview.str_at("tuning_warning").empty())
        add(-1, d.preview.str_at("tuning_warning"));
    add(Reload, "Reload current radio settings");
    add(Cancel, "Cancel setup");
    int selected = 0;
    for (size_t i = 0; i < channel_fields_order_.size(); ++i)
        if (channel_fields_order_[i] == channel_field_)
            selected = static_cast<int>(i);
    if (!d.error.empty()) {
        for (size_t i = 0; i < channel_fields_order_.size(); ++i)
            if (channel_fields_order_[i] >= 0) { selected = static_cast<int>(i); break; }
    }
    const char *title = d.source == "import" && d.preview.int_at("new") > 0
                            ? (d.scanned ? "QR scanned (unsaved)" : "Link loaded (unsaved)")
                            : "Add channel (unsaved)";
    open_picker(title, rows, "flow:form", selected);
}
namespace {
constexpr unsigned kRadioConsent = 1, kAntennaConsent = 2, kPublicConsent = 4, kMqttConsent = 8;
}
bool SettingsScreen::on_key(uint32_t key) {
    if (key != '1' || !channel_flow_ || is_composing())
        return false;
    if (pending_ == "flow:form" && service_route_.empty()) {
        if (channel_draft_.context_ready)
            channel_setup_save();
        else
            view_model().apply_notice("Reload current radio settings first");
    } else if (pending_ == "flow:confirm")
        channel_setup_choice(pending_, -1);
    return true; // Busy/success/sub-pickers never turn a held shortcut into another action.
}
void SettingsScreen::channel_setup_save() {
    auto &d = channel_draft_;
    if (d.source == "import" && d.import_token.empty()) {
        d.error = "Scan or paste a channel first";
        channel_setup_form();
        return;
    }
    required_consent_ = accepted_consent_ = 0;
    const std::string tuning = "{\"region\":" + quote(d.region) + ",\"preset\":" + quote(d.preset) +
                               ",\"use_preset\":" + boolean(d.use_preset) +
                               ",\"slot\":" + std::to_string(d.slot) + ",\"hop\":" + std::to_string(d.hop) +
                               ",\"override_frequency\":" + number(d.override_frequency) + "}";
    request_service(
        "flow:prepare", "channel.setup.preview",
        ",\"source\":" + quote(d.source) + ",\"name\":" + quote(d.name) +
            ",\"import_token\":" + quote(d.import_token) + ",\"base\":" + quote(d.base) +
            ",\"tuning\":" + tuning + ",\"tx\":" + boolean(d.tx) + ",\"position\":" + boolean(d.position) +
            ",\"position_target\":" + std::to_string(d.position_target) + ",\"mqtt\":" + boolean(d.mqtt) +
            ",\"profile_only\":" + boolean(d.profile_only) + ",\"profile_name\":" + quote(d.profile_name));
}
void SettingsScreen::channel_setup_commit() {
    const auto &d = channel_draft_;
    request_service("flow:commit", "channel.setup.commit",
                    ",\"token\":" + quote(d.token) + ",\"confirmed\":true,\"tuning_confirmed\":" +
                        boolean(d.profile_only || (accepted_consent_ & kRadioConsent)) +
                        ",\"antenna_confirmed\":" + boolean(accepted_consent_ & kAntennaConsent) +
                        ",\"public_position_confirmed\":" + boolean(accepted_consent_ & kPublicConsent) +
                        ",\"mqtt_confirmed\":" + boolean(accepted_consent_ & kMqttConsent));
}
void SettingsScreen::channel_setup_ready() {
    auto &d = channel_draft_;
    required_consent_ = accepted_consent_ = 0;
    consent_rows_.clear();
    std::vector<std::string> rows;
    const auto add = [&](const std::string &text, unsigned bit = 0) {
        rows.push_back(text);
        consent_rows_.push_back(bit);
        required_consent_ |= bit;
    };
    add("Back to form (keep entries)");
    if (!d.profile_only && flag(d.result, "tuning_changed")) {
        add("Radio: " + d.region + " / " + d.preset + "; slot " + std::to_string(d.slot) + "; hops " +
            std::to_string(d.hop));
        add("[ ] Apply radio change to every channel", kRadioConsent);
    }
    if (flag(d.result, "tx_confirmation"))
        add("[ ] Antenna attached; enable TX/RX", kAntennaConsent);
    if (flag(d.result, "public_position"))
        add("[ ] Allow public location (~730 m)", kPublicConsent);
    if (flag(d.result, "mqtt"))
        add("[ ] Allow configured MQTT forwarding", kMqttConsent);
    if (!required_consent_) {
        channel_setup_commit();
        return;
    }
    close_overlay();
    open_picker("Confirm these changes", rows, "flow:confirm", 0);
}
void SettingsScreen::channel_setup_finish() {
    close_overlay();
    const auto &r = channel_draft_.result;
    std::vector<std::string> rows = {"Done"};
    if (r.str_at("event") == "error")
        rows.push_back(r.str_at("text"));
    else {
        rows.push_back(flag(r, "profile_only")           ? "Profile saved; active setup unchanged."
                       : r.str_at("verified") == "radio" ? "Saved and verified by the radio."
                                                         : "Saved on device; live check when Radio is On.");
        for (const auto &name : channel_draft_.prepared_names)
            rows.push_back(std::string(flag(r, "profile_only") ? "Includes: " : "Added: ") + name);
        rows.push_back("New channels: " + std::to_string(r.int_at("added")));
        if (!r.str_at("profile").empty())
            rows.push_back("Named profile saved privately.");
        if (!r.str_at("profile_error").empty())
            rows.push_back(r.str_at("profile_error"));
        if (!r.str_at("warning").empty())
            rows.push_back(r.str_at("warning"));
        if (!flag(r, "profile_only") && r.str_at("verified") == "radio")
            rows.push_back("Open conversation");
    }
    open_picker(r.str_at("event") == "error" ? "Save not confirmed" : "Channel setup complete", rows,
                "flow:done", 0);
}
void SettingsScreen::channel_setup_exit() {
    view_model().cancel_service_request();
    channel_flow_ = false;
    service_route_.clear();
    channel_draft_ = {};
    close_overlay();
}
bool SettingsScreen::channel_setup_back() {
    if (service_route_ == "flow:commit") {
        view_model().apply_notice("Save is in progress; wait for confirmation");
        return true;
    }
    if (pending_ == "flow:done") {
        channel_setup_exit();
        return true;
    }
    if (pending_ == "flow:form") {
        close_overlay();
        open_picker("Discard this unsaved setup?", {"Keep editing", "Discard and close"}, "flow:discard", 0);
        return true;
    }
    view_model().cancel_service_request(service_route_ != "flow:scan" && service_route_ != "flow:import");
    channel_draft_.token.clear();
    service_route_.clear();
    if (!channel_draft_.context_ready) {
        channel_setup_exit();
        return true;
    }
    if (overlay_mode_ == Overlay::Text && pending_ == "flow:url" && input_)
        channel_draft_.url = lv_textarea_get_text(input_);
    channel_setup_form();
    return true;
}

bool SettingsScreen::channel_setup_choice(const std::string &pending, int choice) {
    if (pending.rfind("flow:", 0) != 0)
        return false;
    auto &d = channel_draft_;
    const std::string label =
        choice >= 0 && choice < static_cast<int>(options_.size()) ? options_[choice] : "";
    if (pending == "flow:discard") {
        if (choice == 1)
            channel_setup_exit();
        else
            channel_setup_form();
    } else if (pending == "flow:method") {
        view_model().cancel_service_request();
        d.position = false;
        d.position_target = 0;
        d.import_token.clear();
        d.preview = {};
        d.suggested = {};
        d.error.clear();
        close_overlay();
        if (choice < 2) {
            d.source = "import";
            if (!choice)
                open_picker("Aim camera at channel QR", {"Start scan (Return)", "Back to form"},
                            "flow:scan-ready", 0);
            else {
                open_text("Paste share URL (hidden)", d.url, "flow:url");
                lv_textarea_set_password_mode(input_, true);
                lv_textarea_set_max_length(input_, 4096);
            }
        } else {
            d.source = choice == 2 ? "private" : "public";
            d.url.clear();
            channel_field_ = Name;
            channel_setup_form();
        }
    } else if (pending == "flow:scan-ready") {
        if (choice == 0)
            request_service("flow:scan", "channel.scan", ",\"setup\":true");
        else
            channel_setup_form();
    } else if (pending == "flow:form") {
        if (choice < 0 || choice >= static_cast<int>(channel_fields_order_.size()))
            return true;
        channel_field_ = channel_fields_order_[choice];
        d.error.clear();
        close_overlay();
        switch (channel_field_) {
        case Method:
            open_picker(
                "Add a channel",
                {"Scan QR code", "Paste share URL", "Create private channel", "Create public channel"},
                "flow:method", 0);
            break;
        case Name:
            if (d.source == "import") {
                std::vector<std::string> rows = {"Back to form", "Scan / paste again"};
                if (const auto *channels = d.preview.find("channels"))
                    for (const auto &ch : channels->arr)
                        rows.push_back(ch.str_at("name") +
                                       (flag(ch, "private") ? " [Private]" : " [Public]"));
                open_picker("Imported channel details", rows, "flow:import-details", 0);
            } else {
                open_text("Channel name (max 11 UTF-8 bytes)", d.name, "flow:name");
                lv_textarea_set_max_length(input_, 11);
            }
            break;
        case Region:
            open_picker("Region changes every channel", regions, "flow:region", index_of(regions, d.region));
            break;
        case Preset:
            open_picker("Modem changes every channel", presets, "flow:preset", index_of(presets, d.preset));
            break;
        case Slot:
            if (d.override_frequency)
                open_picker("Fixed frequency overrides the slot",
                            {"Keep existing fixed frequency", "Use frequency slots (clear override)"},
                            "flow:frequency-mode", 0);
            else {
                open_text("Frequency slot 0..255 (0=Auto)", std::to_string(d.slot), "flow:slot");
                lv_textarea_set_max_length(input_, 3);
            }
            break;
        case Hop:
            open_picker("Hop limit", {"1", "2", "3", "4", "5", "6", "7"}, "flow:hop", d.hop - 1);
            break;
        case Tx:
            if (d.profile_only) {
                d.error = "Separate profiles start RX only";
                channel_setup_form();
            } else
                open_picker("Transmit mode (whole radio)", {"Receive only", "TX/RX (antenna required)"},
                            "flow:tx", d.tx ? 1 : 0);
            break;
        case Position:
        case Mqtt:
            if (d.profile_only)
                d.error = "Separate profiles start location/MQTT Off";
            else if (channel_field_ == Position) {
                if (d.source == "import" && d.preview.int_at("new") > 1) {
                    std::vector<std::string> choices = {"Off on new channels"};
                    if (const auto *channels = d.preview.find("channels"))
                        for (const auto &ch : channels->arr)
                            if (!flag(ch, "existing"))
                                choices.push_back("Share: " + ch.str_at("name") +
                                                  (flag(ch, "private") ? " [Private]" : " [Public]"));
                    open_picker("Automatic location: choose one channel", choices, "flow:position-target",
                                d.position ? d.position_target + 1 : 0);
                    return true;
                }
                d.position = !d.position;
                d.position_target = 0;
            } else
                d.mqtt = !d.mqtt;
            channel_setup_form();
            break;
        case Destination:
            open_picker("Where should this setup be saved?",
                        {"Apply to current setup", "Separate profile; keep active setup"}, "flow:destination",
                        d.profile_only ? 1 : 0);
            break;
        case Profile:
            open_text("Profile name (empty = no extra profile)", d.profile_name, "flow:profile");
            lv_textarea_set_max_length(input_, 48);
            break;
        case Suggested:
            channel_setup_tuning(d.suggested);
            channel_setup_form();
            break;
        case Reload:
            request_service("flow:context", "channel.setup.context");
            break;
        case Cancel:
            open_picker("Discard this unsaved setup?", {"Keep editing", "Discard and close"}, "flow:discard",
                        0);
            break;
        }
    } else if (pending == "flow:frequency-mode") {
        if (choice == 1) {
            d.override_frequency = 0;
            close_overlay();
            open_text("Frequency slot 0..255 (0=Auto)", std::to_string(d.slot), "flow:slot");
            lv_textarea_set_max_length(input_, 3);
        } else
            channel_setup_form();
    } else if (pending == "flow:region") {
        d.region = label;
        channel_setup_form();
    } else if (pending == "flow:preset") {
        d.preset = label;
        d.use_preset = true;
        channel_setup_form();
    } else if (pending == "flow:hop") {
        d.hop = choice + 1;
        channel_setup_form();
    } else if (pending == "flow:tx") {
        d.tx = choice == 1;
        channel_setup_form();
    } else if (pending == "flow:position-target") {
        d.position = choice > 0;
        d.position_target = choice > 0 ? choice - 1 : 0;
        channel_setup_form();
    } else if (pending == "flow:destination") {
        d.profile_only = choice == 1;
        if (d.profile_only)
            d.tx = d.position = d.mqtt = false;
        channel_setup_form();
    } else if (pending == "flow:import-details") {
        if (choice == 1) {
            close_overlay();
            open_picker(
                "Add a channel",
                {"Scan QR code", "Paste share URL", "Create private channel", "Create public channel"},
                "flow:method", 0);
        } else
            channel_setup_form();
    } else if (pending == "flow:confirm") {
        if (choice == 0)
            channel_setup_form();
        else if (choice < 0) {
            if ((accepted_consent_ & required_consent_) != required_consent_)
                view_model().apply_notice("Confirm the changes above before saving");
            else
                channel_setup_commit();
        } else if (choice > 0 && choice < static_cast<int>(consent_rows_.size()) && consent_rows_[choice]) {
            accepted_consent_ ^= consent_rows_[choice];
            options_[choice].replace(0, 3, (accepted_consent_ & consent_rows_[choice]) ? "[x]" : "[ ]");
            rebuild_picker();
        }
    } else if (pending == "flow:done") {
        if (choice == 0)
            channel_setup_exit();
        else if (label == "Open conversation") {
            std::vector<std::string> rows = {"Back"};
            if (const auto *channels = d.result.find("channels"))
                for (const auto &ch : channels->arr)
                    rows.push_back("Open channel " + std::to_string(ch.int_at("index")));
            close_overlay();
            open_picker("Choose new conversation", rows, "flow:open", 0);
        }
    } else if (pending == "flow:open") {
        if (choice == 0)
            channel_setup_finish();
        else if (const auto *channels = d.result.find("channels");
                 channels && choice - 1 < static_cast<int>(channels->arr.size())) {
            const auto identity = channels->arr[choice - 1].str_at("identity");
            const auto &live = view_model().channels();
            if (std::any_of(live.begin(), live.end(),
                            [&](const auto &ch) { return !identity.empty() && ch.identity == identity; })) {
                channel_setup_exit();
                view_model().set_group_target(identity);
                view_model().show_messages_page();
            } else
                view_model().apply_notice("Channel changed or offline; open Messages after Radio is On");
        }
    }
    return true;
}

bool SettingsScreen::channel_setup_text(const std::string &pending, const std::string &value) {
    if (pending.rfind("flow:", 0) != 0)
        return false;
    auto &d = channel_draft_;
    if (pending == "flow:url") {
        d.url = value;
        request_service("flow:import", "channel.preview", ",\"setup\":true,\"url\":" + quote(value));
        return true;
    }
    if (pending == "flow:name") {
        if (value.size() > 11 || (d.source == "private" && value.empty())) {
            view_model().apply_notice("Use 1..11 UTF-8 bytes for a private name");
            return true;
        }
        d.name = value;
    } else if (pending == "flow:profile") {
        if (value.size() > 48 || (d.profile_only && value.empty())) {
            view_model().apply_notice("Profile name requires 1..48 UTF-8 bytes");
            return true;
        }
        d.profile_name = value;
    } else if (pending == "flow:slot") {
        int slot = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), slot);
        if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
            slot < 0 || slot > 255) {
            view_model().apply_notice("Enter a slot from 0 to 255");
            return true;
        }
        d.slot = slot;
    }
    channel_setup_form();
    return true;
}
void SettingsScreen::channel_setup_result(const std::string &route, const json::Value &event) {
    if (!channel_flow_)
        return;
    auto &d = channel_draft_;
    if (event.str_at("event") == "error") {
        if (route == "flow:commit") {
            d.result = event;
            channel_setup_finish();
        } else {
            d.error = event.str_at("text");
            channel_setup_form();
        }
        return;
    }
    d.error.clear();
    if (route == "flow:context") {
        const auto *tuning = event.find("tuning");
        if (!tuning || !tuning->is_obj() || event.str_at("base").empty()) {
            d.error = "Invalid setup reply; reconnect";
            channel_setup_form();
            return;
        }
        channel_setup_tuning(*tuning);
        d.tx = flag(event, "tx");
        d.base = event.str_at("base");
        d.context_ready = true;
        if (d.source.empty()) {
            close_overlay();
            open_picker(
                "Add a channel",
                {"Scan QR code", "Paste share URL", "Create private channel", "Create public channel"},
                "flow:method", 0);
        } else
            channel_setup_form();
    } else if (route == "flow:scan" || route == "flow:import") {
        if (event.str_at("event") != "channel.preview" || event.int_at("new") <= 0 ||
            event.str_at("token").empty()) {
            d.error = "Already present, or no new channels found";
            channel_setup_form();
            return;
        }
        d.source = "import";
        d.scanned = route == "flow:scan";
        channel_field_ = Name;
        d.import_token = event.str_at("token");
        d.preview = event;
        if (const auto *tuning = event.find("suggested_tuning"))
            d.suggested = *tuning;
        channel_setup_form();
    } else if (route == "flow:prepare") {
        if (event.str_at("event") != "channel.setup.preview" || event.str_at("token").empty()) {
            d.error = "Invalid review reply";
            channel_setup_form();
            return;
        }
        const auto *channels = event.find("channels");
        const bool valid_channels =
            channels && channels->is_arr() && !channels->arr.empty() && channels->arr.size() <= 8 &&
            std::all_of(channels->arr.begin(), channels->arr.end(), [](const auto &ch) {
                const auto *name = ch.find("name");
                const auto *privacy = ch.find("private");
                return ch.is_obj() && name && name->type == json::Value::Str && !name->str.empty() &&
                       name->str.size() <= 80 && privacy && privacy->type == json::Value::Bool;
            });
        if (!valid_channels) {
            d.error = "Invalid channel validation; reconnect";
            channel_setup_form();
            return;
        }
        d.prepared_names.clear();
        for (const auto &ch : channels->arr)
            d.prepared_names.push_back(ch.str_at("name"));
        d.result = event;
        d.token = event.str_at("token");
        channel_setup_ready();
    } else if (route == "flow:commit") {
        if (event.str_at("event") != "channel.setup.saved" ||
            (event.str_at("verified") != "radio" && event.str_at("verified") != "stored") ||
            event.int_at("added") <= 0) {
            json::parse(
                R"({"event":"error","text":"Unexpected save reply; status unknown. Check View channels and Saved mesh setups before retrying."})",
                d.result);
        } else
            d.result = event;
        if (!event.str_at("profile_error").empty())
            view_model().apply_notice(event.str_at("profile_error"));
        if (!event.str_at("warning").empty())
            view_model().apply_notice(event.str_at("warning"));
        channel_setup_finish();
    }
}
} // namespace screen
