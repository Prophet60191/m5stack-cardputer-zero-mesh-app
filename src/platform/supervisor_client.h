/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 *
 * Client for the mesh supervisor's local socket (see
 * ../../docs/software/supervisor-protocol.md). A background thread keeps the
 * connection up and reads event lines into a queue; the UI thread drains the
 * queue with poll() and sends intents with send_line(). Two synthetic events,
 * {"event":"_connected"} and {"event":"_disconnected"}, bracket connectivity so
 * the UI can show an offline state and (re)request a snapshot on connect.
 */
#pragma once

#include "json.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace platform {

class SupervisorClient {
public:
    using Handler = std::function<void(const json::Value&)>;

    SupervisorClient();
    ~SupervisorClient();

    SupervisorClient(const SupervisorClient&) = delete;
    SupervisorClient& operator=(const SupervisorClient&) = delete;

    // Start the connect/read loop. `on_event` is called on the UI thread from poll().
    void start(Handler on_event);
    void stop();

    // Drain queued events on the UI thread (call each UI tick).
    void poll();

    // Send one intent line (a JSON object). Safe from the UI thread; a no-op if offline.
    void send_line(const std::string& line);

    bool connected() const { return connected_.load(); }

private:
    void run();                 // background: connect, read, reconnect
    bool connect_once();        // returns true if connected
    void read_loop();           // read lines until the socket closes

    static std::string socket_path();

    Handler on_event_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    std::mutex fd_mutex_;       // guards fd_ for send
    int fd_ = -1;

    std::mutex queue_mutex_;
    std::deque<json::Value> queue_;

    void enqueue(json::Value&& v);
    void enqueue_event(const char* event);
};

}  // namespace platform
