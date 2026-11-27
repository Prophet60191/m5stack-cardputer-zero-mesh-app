// SPDX-License-Identifier: MIT
#include "system_keys.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <iostream>
#undef assert
#define assert(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::abort();}}while(0)
using namespace cp0keys;
struct Fake:Backend {
 std::atomic<unsigned> count{0};std::thread::id owner=std::this_thread::get_id();
 Result perform(Action a)override{assert(std::this_thread::get_id()!=owner);++count;return {true,std::to_string(int(a))};}
};
static Result wait(Controls& c){for(int i=0;i<500;++i){if(auto r=c.poll())return *r;std::this_thread::sleep_for(std::chrono::milliseconds(2));}assert(false);return {};}
int main(){
 const unsigned codes[]={113,114,115,165,164,163,224,225};
 for(unsigned i=0;i<8;++i){assert(action_for(codes[i])==Action(i));auto b=std::make_shared<Fake>();Controls c(b);assert(!c.key(30,1));assert(c.key(codes[i],1));assert(c.key(codes[i],0));assert(wait(c).ok);assert(b->count==1);}
 for(unsigned code:{113u,163u,164u,165u}){auto b=std::make_shared<Fake>();Controls c(b);c.key(code,1);wait(c);for(int i=0;i<100;++i)c.key(code,2);c.key(code,0);assert(b->count==1);}
 struct Failing:Backend{Result perform(Action)override{throw std::runtime_error("private error must not leak");}};
 Controls fail(std::make_shared<Failing>());fail.key(115,1);auto r=wait(fail);assert(!r.ok&&r.text=="System control unavailable");
 struct Slow:Backend{std::atomic<bool> enter{false},release{false};std::atomic<unsigned> count{0};Result perform(Action)override{enter=true;while(!release)std::this_thread::yield();++count;return {true,"done"};}};
 auto b=std::make_shared<Slow>();{Controls c(b);c.key(115,1);while(!b->enter)std::this_thread::yield();for(int i=0;i<10000;++i)c.key(115,2);c.key(115,0);b->release=true;wait(c);assert(b->count==1);}
 assert(!action_for(0)&&!action_for(1000000));std::cout<<"All 8 system actions, worker dispatch, repeats, release, failure and bounds pass\n";
}
