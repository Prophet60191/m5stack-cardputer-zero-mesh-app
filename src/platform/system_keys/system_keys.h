// SPDX-License-Identifier: MIT
#pragma once
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
namespace cp0keys {
enum class Action { Mute, VolumeDown, VolumeUp, Previous, PlayPause, Next, BrightnessDown, BrightnessUp };
std::optional<Action> action_for(unsigned code);
struct Result { bool ok=false; std::string text; };
struct Backend { virtual ~Backend()=default; virtual Result perform(Action)=0; };
std::shared_ptr<Backend> native_backend();
class Controls {
 public:
  explicit Controls(std::shared_ptr<Backend> backend=native_backend());
  ~Controls();
  Controls(const Controls&)=delete; Controls& operator=(const Controls&)=delete;
  bool key(unsigned code,int value);
  std::optional<Result> poll();
 private:
  void run();
  std::shared_ptr<Backend> backend_;
  std::mutex mutex_;std::condition_variable ready_;std::thread worker_;
  struct Job { Action action; bool repeated; };
  std::deque<Job> pending_;std::deque<Result> results_;bool stopping_=false;
};
}
extern "C" {
void* cp0_system_keys_create();
void cp0_system_keys_destroy(void*);
int cp0_system_keys_handle(void*,unsigned,int);
const char* cp0_system_keys_poll(void*);
}
