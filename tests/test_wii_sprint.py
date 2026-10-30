"""Run the actual double-tap detector on the host, without Wii SDK or game."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
test=r'''
#include <assert.h>
#include <stdio.h>
#include "sprint_wii.h"
static wii_sprint_gesture_t s;
static int poll(unsigned t,float x,float y,int on,unsigned source){
 return Wii_SprintGesture(&s,source,on,x,y,t);
}
static void reset(unsigned t){s=(wii_sprint_gesture_t){0};assert(!poll(t,0,0,1,1));assert(!poll(t+1,0,0,1,1));}
int main(void){
 reset(0);
 assert(!poll(20,0,1,1,1)); /* one push walks */
 for(unsigned t=30;t<300;t+=10)assert(!poll(t,0,1,1,1));
 assert(!poll(300,0,0,1,1));assert(poll(350,0,1,1,1));
 for(unsigned t=360;t<2000;t+=10)assert(!poll(t,0,1,1,1));
 assert(!poll(2000,0,0,1,1));assert(!poll(2020,0,1,1,1));
 assert(!poll(2040,0,0,1,1));assert(poll(2060,0,1,1,1));
 reset(0);assert(!poll(20,0,1,1,1));assert(!poll(40,0,0,1,1));
 assert(!poll(500,0,1,1,1)); /* expired first tap */
 reset(0);assert(!poll(20,0,.3f,1,1));assert(!poll(240,0,.8f,1,1));
 assert(!poll(260,0,0,1,1));assert(!poll(280,0,1,1,1)); /* slow first push */
 reset(0);assert(!poll(20,0,1,1,1));assert(!poll(40,0,0,1,1));
 assert(!poll(60,0,1,0,1));assert(!poll(80,0,1,1,1)); /* menu/aim/death */
 assert(!poll(100,0,0,1,1));assert(!poll(120,0,1,1,1));
 reset(0);assert(!poll(20,0,1,1,1));assert(!poll(40,0,0,1,1));
 assert(!poll(60,0,1,1,2));assert(!poll(80,0,1,1,2)); /* source switch */
 reset(0);assert(!poll(20,0,-1,1,1));assert(!poll(40,0,0,1,1));assert(!poll(60,0,1,1,1));
 reset(0);assert(!poll(20,1,1,1,1));assert(!poll(40,0,0,1,1));assert(!poll(60,0,1,1,1));
 reset(0);for(unsigned t=10;t<200;t+=10)assert(!poll(t,.1f,.1f,1,1));
 assert(!poll(220,0,1,1,1));assert(!poll(240,0,0,1,1));
 assert(!poll(260,0,0,1,0));assert(!poll(280,0,1,1,1)); /* disconnect */
 reset(UINT32_MAX-100);assert(!poll(UINT32_MAX-80,0,1,1,1));
 assert(!poll(UINT32_MAX-60,0,0,1,1));assert(poll(20,0,1,1,1));
 puts("PASS: double tap, held walking, timeout, slow push, blocked input, source change, backwards/sideways, jitter, disconnect, clock wrap");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-sprint-') as tmp:
 p=Path(tmp)/'test.c'; exe=Path(tmp)/'test'
 p.write_text(test,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g',
  '-fsanitize=address,undefined','-fno-omit-frame-pointer',
  '-I',str(root/'source/platform/wii'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
