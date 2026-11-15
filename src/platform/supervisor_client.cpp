/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // SO_PEERCRED / struct ucred on Linux
#endif
#include "supervisor_client.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace platform {

SupervisorClient::SupervisorClient() = default;

SupervisorClient::~SupervisorClient() {
    stop();
}

std::string SupervisorClient::socket_path() {
    if (const char* run = std::getenv("XDG_RUNTIME_DIR")) {
        if (run[0] != '\0') {
            return std::string(run) + "/cz-mesh/supervisor.sock";
        }
    }
    return "/tmp/cz-mesh-" + std::to_string(getuid()) + "/supervisor.sock";
}

void SupervisorClient::start(Handler on_event) {
    on_event_ = std::move(on_event);
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void SupervisorClient::stop() {
    running_ = false;
    {
        std::lock_guard<std::mutex> lock(fd_mutex_);
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
        }
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void SupervisorClient::run() {
    while (running_) {
        if (connect_once()) {
            enqueue_event("_connected");
            connected_ = true;
            read_loop();
            connected_ = false;
            enqueue_event("_disconnected");
        }
        {
            std::lock_guard<std::mutex> lock(fd_mutex_);
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
        }
        if (!running_) break;
        // Retry so the UI survives the supervisor starting later or restarting.
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

bool SupervisorClient::connect_once() {
    const std::string path = socket_path();
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return false;
    }
#ifdef __linux__
    // Anti-impersonation: only talk to a supervisor owned by us. On the /tmp
    // fallback path another local user could pre-bind the socket otherwise.
    struct ucred cred {};
    socklen_t cred_len = sizeof(cred);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) != 0 ||
        cred.uid != getuid()) {
        ::close(fd);
        return false;
    }
#endif
    // Bound send time so a peer that stops reading can't freeze the UI thread.
    timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#ifdef SO_NOSIGPIPE
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    std::lock_guard<std::mutex> lock(fd_mutex_);
    fd_ = fd;
    return true;
}

void SupervisorClient::read_loop() {
    std::string buffer;
    char chunk[1024];
    while (running_) {
        int fd;
        {
            std::lock_guard<std::mutex> lock(fd_mutex_);
            fd = fd_;
        }
        if (fd < 0) break;
        ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n <= 0) {
            break;  // closed or error
        }
        buffer.append(chunk, static_cast<size_t>(n));
        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (line.empty()) continue;
            json::Value v;
            if (json::parse(line, v) && v.is_obj()) {
                enqueue(std::move(v));
            }
        }
    }
}

void SupervisorClient::send_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(fd_mutex_);
    if (fd_ < 0) {
        return;  // offline: drop
    }
    std::string payload = line;
    payload.push_back('\n');
    ssize_t off = 0;
    const ssize_t total = static_cast<ssize_t>(payload.size());
    while (off < total) {
        ssize_t w = ::send(fd_, payload.data() + off, static_cast<size_t>(total - off),
                           MSG_NOSIGNAL);   // never raise SIGPIPE on a closed peer
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            break;   // closed, error, or send timed out -> drop (bounded, no freeze)
        }
        off += w;
    }
}

void SupervisorClient::enqueue(json::Value&& v) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.push_back(std::move(v));
}

void SupervisorClient::enqueue_event(const char* event) {
    json::Value v;
    v.type = json::Value::Obj;
    json::Value e;
    e.type = json::Value::Str;
    e.str = event;
    v.obj["event"] = std::move(e);
    enqueue(std::move(v));
}

void SupervisorClient::poll() {
    for (;;) {
        json::Value v;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.empty()) break;
            v = std::move(queue_.front());
            queue_.pop_front();
        }
        if (on_event_) {
            on_event_(v);
        }
    }
}

}  // namespace platform
