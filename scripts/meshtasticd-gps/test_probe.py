# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual pinned native probe parser with synthetic delayed replies."""
from pathlib import Path
import sys
import re,subprocess,tempfile,json
source=Path(sys.argv[1]).resolve()
s=(source/'src/gps/GPS.cpp').read_text()
start=s.index('GnssModel_t GPS::getProbeResponse(');opening=s.index('{',start);depth=1;end=opening+1
while depth:
 depth+=(s[end]=='{')-(s[end]=='}');end+=1
function=s[start:end]
selected=int(re.search(r'PROBE_FAMILY\("ATGM33xx Family", "\$PCAS06,1\*1A", atgm, (\d+)\)',s).group(1))
assert selected==1500
prefix=r'''
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <cassert>
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
unsigned long clockMs=0;
unsigned long millis(){return clockMs++;}
enum GnssModel_t { GNSS_MODEL_UNKNOWN, GNSS_MODEL_ATGM336H };
struct ChipInfo { std::string chipName,detectionString;GnssModel_t driver; };
struct Serial {
 unsigned long arrival;std::string data;size_t offset=0;
 bool available(){return clockMs>=arrival && offset<data.size();}
 char read(){return data[offset++];}
};
struct GPS {
 Serial *_serial_gps;
 GnssModel_t getProbeResponse(unsigned long,const std::vector<ChipInfo>&,int);
};
'''
suffix=r'''
const std::string identity="$GPTXT,01,01,02,HW=ATGM336H-6N*00\r\n";
GnssModel_t check(unsigned long arrival,unsigned long timeout,std::string data=identity,int baud=115200){
 clockMs=0;Serial serial{arrival,data};GPS gps{&serial};
 std::vector<ChipInfo> chips{{"ATGM336H","$GPTXT,01,01,02,HW=ATGM336H",GNSS_MODEL_ATGM336H}};
 auto result=gps.getProbeResponse(timeout,chips,baud);
 assert(clockMs<=timeout+3);return result;
}
int main(){
 int oldPass=0,newPass=0;
 for(auto delay:{98UL,613UL,977UL,771UL,772UL,770UL,771UL,772UL}){
  oldPass+=check(delay,500)==GNSS_MODEL_ATGM336H;
  newPass+=check(delay,PROBE_TIMEOUT)==GNSS_MODEL_ATGM336H;
 }
 assert(oldPass==1 && newPass==8);
 assert(check(1600,PROBE_TIMEOUT)==GNSS_MODEL_UNKNOWN);
 assert(check(100,PROBE_TIMEOUT,"$GPTXT,01,01,02,HW=Unrelated*00\r\n")==GNSS_MODEL_UNKNOWN);
 assert(check(100,PROBE_TIMEOUT,std::string(3000,'X')+"\r\n"+identity,9600)==GNSS_MODEL_UNKNOWN);
 std::cout<<"actual native parser: old deadline 1/8; corrected deadline 8/8; timeout/unknown/overflow guards passed\n";
}
'''.replace('PROBE_TIMEOUT',str(selected))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'probe.cpp';p.write_text(prefix+function+suffix)
 binary=Path(tmp)/'probe'
 subprocess.run(['g++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
