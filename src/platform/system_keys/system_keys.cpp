// SPDX-License-Identifier: MIT
#include "system_keys.h"
#include "upstream/cp0_audio_mixer_control.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <vector>
#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace cp0keys {
std::optional<Action> action_for(unsigned code){
 switch(code){case 113:return Action::Mute;case 114:return Action::VolumeDown;case 115:return Action::VolumeUp;
 case 163:return Action::Next;case 164:return Action::PlayPause;case 165:return Action::Previous;
 case 224:return Action::BrightnessDown;case 225:return Action::BrightnessUp;default:return {};}
}
namespace {
#ifdef __linux__
// Fixed executable and typed arguments only. Bounded output/time; no shell.
std::optional<std::string> bus_call(const std::string& destination,const std::string& path,const std::string& method){
 int pipefd[2];if(pipe2(pipefd,O_CLOEXEC|O_NONBLOCK))return {};
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDOUT_FILENO);
 posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
 posix_spawn_file_actions_addclose(&actions,pipefd[0]);posix_spawn_file_actions_addclose(&actions,pipefd[1]);
 posix_spawnattr_t attr;posix_spawnattr_init(&attr);posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attr,0);
 std::vector<std::string> args={"/usr/bin/gdbus","call","--session","--dest",destination,"--object-path",path,"--method",method};
 std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);
 pid_t child=-1;int rc=posix_spawn(&child,args[0].c_str(),&actions,&attr,argv.data(),environ);
 posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attr);close(pipefd[1]);
 if(rc){close(pipefd[0]);return {};}
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);std::string output;int status=0;bool done=false;
 while(std::chrono::steady_clock::now()<deadline){
  char buf[1024];ssize_t n=read(pipefd[0],buf,sizeof(buf));if(n>0){output.append(buf,n);if(output.size()>16384)break;}
  auto waited=waitpid(child,&status,WNOHANG);if(waited==child){done=true;while((n=read(pipefd[0],buf,sizeof(buf)))>0){output.append(buf,n);if(output.size()>16384)break;}break;}
  if(waited<0&&errno!=EINTR)break;
  pollfd event{pipefd[0],POLLIN,0};poll(&event,1,10);
 }
 if(!done){kill(-child,SIGKILL);while(waitpid(child,&status,0)<0&&errno==EINTR){}}
 close(pipefd[0]);if(!done||!WIFEXITED(status)||WEXITSTATUS(status)!=0||output.size()>16384)return {};return output;
}
#endif
class Native: public Backend {
 Cp0AudioMixerControl mixer_;
 public: Result perform(Action a) override {
#ifdef __linux__
  if(geteuid()==0)return {false,"System keys need the normal user"};
  if(a==Action::Mute){int mute=mixer_.toggle_mute();return mute<0?Result{false,"Audio unavailable"}:Result{true,mute?"Muted":"Unmuted"};}
  if(a==Action::VolumeDown||a==Action::VolumeUp){
   int old=mixer_.read_volume();if(old<0)return {false,"Audio unavailable"};
   int now=mixer_.write_volume(std::clamp(old+(a==Action::VolumeUp?10:-10),0,100));
   return now<0?Result{false,"Volume change failed"}:Result{true,"Volume "+std::to_string(now)+"%"};
  }
  if(a==Action::BrightnessDown||a==Action::BrightnessUp){
   constexpr auto base="/sys/class/backlight/backlight/";int raw=-1,maximum=0;
   std::ifstream(std::string(base)+"brightness")>>raw;std::ifstream(std::string(base)+"max_brightness")>>maximum;
   if(raw<0||maximum<1||maximum>65535)return {false,"Brightness unavailable"};
   int target=std::clamp((raw*100+maximum/2)/maximum+(a==Action::BrightnessUp?10:-10),10,100);
   {std::ofstream file(std::string(base)+"brightness");file<<(target*maximum+50)/100;file.flush();if(!file)return {false,"Brightness change failed"};}
   int actual=-1;std::ifstream(std::string(base)+"brightness")>>actual;
   if(actual!=(target*maximum+50)/100)return {false,"Brightness not confirmed"};
   return {true,"Brightness "+std::to_string(target)+"%"};
  }
  auto names=bus_call("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus.ListNames");
  if(!names)return {false,"Media controls unavailable"};
  std::vector<std::string> players;const std::string prefix="org.mpris.MediaPlayer2.";size_t pos=0;
  while((pos=names->find(prefix,pos))!=std::string::npos){size_t end=pos+prefix.size();while(end<names->size()&&(((*names)[end]>='a'&&(*names)[end]<='z')||((*names)[end]>='A'&&(*names)[end]<='Z')||((*names)[end]>='0'&&(*names)[end]<='9')||(*names)[end]=='_'||(*names)[end]=='.'))++end;
   const auto name=names->substr(pos,end-pos);if(name.size()>prefix.size()&&name.size()<=255)players.push_back(name);pos=end;}
  if(players.empty())return {false,"No active media player"};if(players.size()!=1)return {false,"Choose one media player"};
  std::string method="org.mpris.MediaPlayer2.Player.";method+=a==Action::Next?"Next":a==Action::Previous?"Previous":"PlayPause";
  return bus_call(players[0],"/org/mpris/MediaPlayer2",method)?Result{true,"Media command sent"}:Result{false,"Media command failed"};
#else
  (void)a;return {false,"System control is device-only"};
#endif
 }
};
}
std::shared_ptr<Backend> native_backend(){return std::make_shared<Native>();}
Controls::Controls(std::shared_ptr<Backend> backend):backend_(std::move(backend)){}
Controls::~Controls(){{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;pending_.clear();}ready_.notify_all();if(worker_.joinable())worker_.join();}
bool Controls::key(unsigned code,int value){
 auto action=action_for(code);if(!action)return false;
 if(value<0||value>2)return true;
 if(value==0){std::lock_guard<std::mutex> lock(mutex_);pending_.erase(std::remove_if(pending_.begin(),pending_.end(),[&](const Job& job){return job.repeated&&job.action==*action;}),pending_.end());return true;}
 if(value==2&&(*action==Action::Mute||*action==Action::PlayPause||*action==Action::Next||*action==Action::Previous))return true;
 std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return true;
 if(value==2&&std::any_of(pending_.begin(),pending_.end(),[&](const Job& job){return job.repeated&&job.action==*action;}))return true;
 if(pending_.size()<8)pending_.push_back({*action,value==2});
 if(!worker_.joinable())worker_=std::thread([this]{run();});ready_.notify_one();return true;
}
void Controls::run(){
 while(true){Action action;{std::unique_lock<std::mutex> lock(mutex_);ready_.wait(lock,[&]{return stopping_||!pending_.empty();});if(stopping_)return;action=pending_.front().action;pending_.pop_front();}
  Result result;try{result=backend_->perform(action);}catch(...){result={false,"System control unavailable"};}
  std::lock_guard<std::mutex> lock(mutex_);if(results_.size()==8)results_.pop_front();results_.push_back(std::move(result));
 }
}
std::optional<Result> Controls::poll(){std::lock_guard<std::mutex> lock(mutex_);if(results_.empty())return {};auto result=std::move(results_.front());results_.pop_front();return result;}
}
namespace {struct CHandle {cp0keys::Controls controls;std::string result;};}
extern "C" void* cp0_system_keys_create(){try{return new CHandle;}catch(...){return nullptr;}}
extern "C" void cp0_system_keys_destroy(void* p){delete static_cast<CHandle*>(p);}
extern "C" int cp0_system_keys_handle(void* p,unsigned code,int value){if(!p)return 0;try{return static_cast<CHandle*>(p)->controls.key(code,value);}catch(...){return 0;}}
extern "C" const char* cp0_system_keys_poll(void* p){if(!p)return nullptr;try{auto& h=*static_cast<CHandle*>(p);auto r=h.controls.poll();if(!r)return nullptr;h.result=r->text;return h.result.c_str();}catch(...){return nullptr;}}
