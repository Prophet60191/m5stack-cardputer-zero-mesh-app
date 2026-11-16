/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 *
 * Client for the mesh supervisor's local socket (see
 * ../../docs/supervisor-protocol.md). A background thread keeps the
 * connection up and reads event lines into a queue; the UI thread drains the
 * queue with poll() and sends intents with send_line(). Two synthetic events,
 * {"event":"_connected"} and {"event":"_disconnected"}, bracket connectivity so
 * the UI can show an offline state and (re)request a snapshot on connect.
 */
#pragma once

#include "json.h"

#include <atomic>
#include <condition_variable>
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

    // Send one intent line (a JSON object). Safe from the UI thread; returns false if offline or full.
    bool send_line(const std::string& line);

    bool connected() const { return connected_.load(); }

private:
    friend struct SupervisorClientTest;
    void run();                 // background: connect, read, reconnect
    bool connect_once();        // returns true if connected
    void read_loop();           // read lines until the socket closes

    static std::string socket_path();

    Handler on_event_;
    std::thread thread_;
    std::thread writer_;
    std::mutex write_mutex_;
    std::condition_variable write_ready_;
    struct Write { std::string line; unsigned generation; };
    std::deque<Write> writes_;
    size_t write_bytes_ = 0;
    std::atomic<unsigned> generation_{0};
    std::atomic<bool> overflow_{false};
    void write_loop();
    void failed_send(const std::string& line);
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    std::mutex fd_mutex_;       // guards fd_ for send
    int fd_ = -1;

    std::mutex queue_mutex_;
    std::deque<std::pair<json::Value, size_t>> queue_;
    size_t queue_bytes_ = 0;

    bool enqueue(json::Value&& v, size_t bytes = 256);
    void enqueue_event(const char* event);
};

}  // namespace platform
