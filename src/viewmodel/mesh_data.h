/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 *
 * Plain data the screens render. Mirrors the supervisor's event payloads
 * (see ../../docs/software/supervisor-protocol.md).
 */
#pragma once

#include <cstdlib>
#include <string>
#include <vector>

namespace viewmodel {

struct NodeInfo {
    std::string id;
    std::string name;
    std::string shortName;
    double snr = 0.0;
    bool hasSnr = false;
    long lastHeard = 0;
    int hops = -1;
    bool isSelf = false;
    bool favorite = false;      // app-local favourite
    bool hasBattery = false;
    int battery = 0;            // percent
    double voltage = 0.0;
    bool hasPos = false;
    double lat = 0.0;
    double lon = 0.0;
};

struct ChannelInfo {
    int index = 0;
    std::string name;   // empty = the default/primary channel
    std::string role;   // "PRIMARY" | "SECONDARY"
};

struct MessageInfo {
    std::string from;
    std::string fromName;
    std::string to;          // "broadcast" or a node id (for DM threading)
    std::string text;
    std::string id;          // client/packet id, for delivery tracking
    int channel = 0;
    long time = 0;
    bool outgoing = false;
    int status = 0;          // outgoing: 1 sending, 2 delivered, 3 failed; 0 = n/a
    // Signal provenance for received messages (surfaced in message details).
    double snr = 0.0;
    bool hasSnr = false;
    int hops = -1;           // hops away (-1 = unknown / direct)
    std::string relay;       // id of the node that relayed it to us (optional)
};

// Delivery status values for MessageInfo::status.
inline constexpr int kMsgSending = 1;
inline constexpr int kMsgDelivered = 2;
inline constexpr int kMsgFailed = 3;

// The one canonical thread key for a message. Broadcasts are keyed by CHANNEL
// ("ch:<index>") so a public-channel broadcast can never share a thread with a
// private channel (and a reply can never be routed to the wrong channel);
// direct messages are "dm:<peer-id>". Defined in exactly one place.
inline std::string thread_key_of(const MessageInfo& m) {
    if (m.to == "broadcast" || m.to.empty()) return std::string("ch:") + std::to_string(m.channel);
    return std::string("dm:") + (m.outgoing ? m.to : m.from);
}

// True if a thread key names a broadcast/group channel.
inline bool is_group_thread(const std::string& key) {
    return key.rfind("ch:", 0) == 0;
}

// The channel index a group thread key names (-1 if not a group thread).
inline int group_thread_channel(const std::string& key) {
    if (!is_group_thread(key)) return -1;
    return std::atoi(key.c_str() + 3);
}

}  // namespace viewmodel
