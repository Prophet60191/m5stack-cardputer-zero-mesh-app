/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   // SO_PEERCRED / struct ucred on Linux
#endif
#include "supervisor_client.h"

#include <poll.h>
#include <fcntl.h>
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
    if (running_.exchange(true)) return;
    writer_ = std::thread([this] { write_loop(); });
    thread_ = std::thread([this] { run(); });
}

void SupervisorClient::stop() {
    running_ = false;
    write_ready_.notify_all();
    {
        std::lock_guard<std::mutex> lock(fd_mutex_);
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
        }
    }
    if (thread_.joinable()) thread_.join();
    if (writer_.joinable()) writer_.join();
}

void SupervisorClient::run() {
    while (running_) {
        overflow_ = false;
        if (connect_once()) {
            ++generation_;
            connected_ = true;
            enqueue_event("_connected");
            read_loop();
            connected_ = false;
            ++generation_;
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
    if (path.size() >= sizeof(sockaddr_un::sun_path)) { ::close(fd); return false; }
    if (::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) { ::close(fd); return false; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (errno != EINPROGRESS) { ::close(fd); return false; }
        pollfd pending{fd, POLLOUT, 0};
        int error = 0; socklen_t length = sizeof(error);
        if (::poll(&pending, 1, 500) <= 0 ||
            ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
            ::close(fd); return false;
        }
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
        if (overflow_) break;
        pollfd input{fd, POLLIN, 0};
        const int ready = ::poll(&input, 1, 250);
        if (ready == 0) continue;
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n <= 0) {
            break;  // closed or error
        }
        buffer.append(chunk, static_cast<size_t>(n));
        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            if (nl > 2 * 1024 * 1024) return;
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (line.empty()) continue;
            json::Value v;
            if (json::parse(line, v) && v.is_obj()) {
                if (!enqueue(std::move(v), line.size())) return;
            }
        }
        if (buffer.size() > 2 * 1024 * 1024) return;
    }
}

bool SupervisorClient::send_line(const std::string& line) {
    const unsigned generation = generation_.load();
    if (!connected_ || line.size() > 64 * 1024 || line.find('\n') != std::string::npos) return false;
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (writes_.size() >= 64 || write_bytes_ + line.size() > 64 * 1024) return false;
    if (!connected_ || generation_ != generation) return false;
    writes_.push_back({line, generation});
    write_bytes_ += line.size();
    write_ready_.notify_one();
    return true;
}

void SupervisorClient::failed_send(const std::string& line) {
    json::Value v;
    if (!json::parse(line, v) || !v.is_obj()) return;
    // Report on the UI thread. Never replay a failed operation on reconnect.
    json::Value event; event.type = json::Value::Str; event.str = "error";
    v.obj["event"] = event;
    event.str = "Connection lost; action was not confirmed";
    v.obj["text"] = event;
    v.obj.erase("value"); v.obj.erase("url");
    enqueue(std::move(v), line.size());
}

void SupervisorClient::write_loop() {
    while (running_) {
        Write work;
        {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_ready_.wait(lock, [this] { return !running_ || !writes_.empty(); });
            if (!running_) return;
            work = std::move(writes_.front()); writes_.pop_front();
            write_bytes_ -= work.line.size();
        }
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(fd_mutex_);
            if (connected_ && fd_ >= 0 && generation_ == work.generation) {
                const std::string payload = work.line + "\n";
                size_t off = 0;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                while (running_ && off < payload.size() && std::chrono::steady_clock::now() < deadline) {
                    const ssize_t n = ::send(fd_, payload.data() + off, payload.size() - off,
                                             MSG_NOSIGNAL | MSG_DONTWAIT);
                    if (n > 0) { off += static_cast<size_t>(n); continue; }
                    if (n < 0 && errno == EINTR) continue;
                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        pollfd output{fd_, POLLOUT, 0}; ::poll(&output, 1, 25); continue;
                    }
                    break;
                }
                ok = off == payload.size();
                if (!ok) {
                    connected_ = false;
                    ::shutdown(fd_, SHUT_RDWR);  // a partial frame poisons this stream
                }
            }
        }
        if (!ok) failed_send(work.line);
    }
}

bool SupervisorClient::enqueue(json::Value&& v, size_t bytes) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    bytes = v.memory_size();
    if (queue_.size() >= 128 || queue_bytes_ + bytes > 16 * 1024 * 1024) {
        queue_.clear(); queue_bytes_ = 0;
        overflow_ = true;
        return false;
    }
    queue_bytes_ += bytes;
    queue_.emplace_back(std::move(v), bytes);
    return true;
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
    for (int processed = 0; processed < 32; ++processed) {
        json::Value v;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.empty()) break;
            queue_bytes_ -= queue_.front().second;
            v = std::move(queue_.front().first);
            queue_.pop_front();
        }
        if (on_event_) {
            on_event_(v);
        }
    }
}

}  // namespace platform
