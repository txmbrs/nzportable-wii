"""Production QC checkpoints/VM gate/loading pump; mocked network, no game VM."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def extract(path,signature):
 s=(root/path).read_text(encoding='utf-8');a=s.index(signature);b=s.index('\n}',a)+2;return s[a:b]
gate=extract('fte-online/fteqw-master/engine/client/pr_csqc.c','void CL_WiiQCLibPoll(pubprogfuncs_t *vm)\n')
pump=extract('fte-online/fteqw-master/engine/client/cl_main.c','void CL_WiiLoadingKeepalive(void)\n')
stub=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#define PLATFORM_WII 1
#define CLIENTONLY 1
#define true 1
#define false 0
#define CP_QUAKEWORLD 1
enum {ca_disconnected,ca_connected,ca_onserver,ca_active};
typedef int qboolean;
typedef struct {int id;} pubprogfuncs_t;
#include "wii_cooperate.h"
static pubprogfuncs_t game={1},menu={2},*csqcprogs=&game;
static struct {int demoplayback,protocol,state,sockets,netchan;} cls={0,1,ca_onserver,0,0};
static unsigned now,reads,ticks,sends,last_read,max_read_gap;
static int main_thread=1,wii_game_read_depth,wii_transport_pump;
static size_t wii_load_bytes;
void CL_WiiLoadingKeepalive(void);
static int Sys_IsMainThread(void){return main_thread;}
static unsigned Sys_Milliseconds(void){return now;}
static void NET_ReadPackets(int sockets){(void)sockets;assert(wii_transport_pump && !wii_game_read_depth);++reads;
 if(last_read && now-last_read>max_read_gap)max_read_gap=now-last_read;last_read=now;
 CL_WiiQCLibPoll(csqcprogs); /* Reentrant transport callback must be gated. */
}
static void NET_Tick(void){assert(wii_transport_pump);++ticks;}
static int Netchan_Transmit(int *chan,int length,void *data,int rate){(void)chan;(void)rate;assert(!length && !data);++sends;return 12;}
static void Con_Printf(const char *fmt,...){(void)fmt;}
'''
main=r'''
int main(void){
 uint64_t sum=0,expected=0;unsigned counter=0;
 for(unsigned i=1;i<=1500000;++i){now=i/256;sum+=i;expected+=i;WII_QC_COOPERATE(&game,++counter,4095u);}
 assert(sum==expected && reads>30 && ticks==reads && sends>=5 && max_read_gap<=116);
 unsigned before=reads;now+=1000;CL_WiiQCLibPoll(&menu);CL_WiiQCLibPoll(NULL);assert(reads==before);
 cls.state=ca_active;CL_WiiQCLibPoll(&game);assert(reads==before);
 cls.state=ca_onserver;main_thread=0;CL_WiiQCLibPoll(&game);assert(reads==before);main_thread=1;
 wii_game_read_depth=1;CL_WiiQCLibPoll(&game);assert(reads==before);
 wii_game_read_depth=0;now+=1000;CL_WiiQCLibPoll(&game);assert(reads==before+1);
 puts("PASS: actual QC checkpoint macro, game-VM/sign-on gating, long CPU work, bounded mock-time network gaps, no nested receive or game-parser entry");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-qc-cooperate-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test';p.write_text(stub+'\n'+gate+'\n'+pump+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'fte-online/fteqw-master/engine/qclib'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 p.write_text('#include <assert.h>\ntypedef struct {int id;} pubprogfuncs_t;\n#include "wii_cooperate.h"\nint main(void){int untouched=0;WII_QC_COOPERATE((pubprogfuncs_t*)0,++untouched,0u);assert(!untouched);return 0;}\n',encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-I',str(root/'fte-online/fteqw-master/engine/qclib'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('PASS: non-Wii checkpoint is a no-op without an engine callback dependency')
