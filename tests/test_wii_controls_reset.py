"""Run the actual controls reset against dirty bindings/settings on the host."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'source/platform/wii/controls_defaults_wii.c').read_text(encoding='utf-8')
src=re.sub(r'^#include[^\n]*\n','',src,flags=re.M)
stub=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "controls_defaults_wii.h"
#define MAX_KEYS 128
#define false 0
static int cmd_source,src_command,in_bind=1,keydown[MAX_KEYS],saved,cleared,released;
int bindings_currentwatch=7;
static char normal[MAX_KEYS][64],hold[MAX_KEYS][64],dt[MAX_KEYS][64];
static char names[MAX_KEYS][64],values[32][32];static int count;
static int Key_StringToKeynum(char *s){for(int i=0;i<count;++i)if(!strcmp(names[i],s))return i;assert(count<MAX_KEYS);strcpy(names[count],s);return count++;}
static void Key_SetBinding(int key,char *s){assert(key>=0);strcpy(normal[key],s);}
static void Key_SetHoldBinding(int key,char *s){assert(key>=0);strcpy(hold[key],s);}
static void Key_SetDTBinding(int key,char *s){assert(key>=0);strcpy(dt[key],s);}
static void Key_Event(int key,int down){assert(!down);assert(!strcmp(normal[key],"+attack"));++released;keydown[key]=0;}
static void Key_ClearStates(void){memset(keydown,0,sizeof(keydown));}
static void IN_PlatformClearPendingInput(void){++cleared;}
static void Cvar_Set(const char *name,char *value){
 for(unsigned i=0;i<sizeof(wii_factory_controls)/sizeof(wii_factory_controls[0]);++i)
  if(!strcmp(name,wii_factory_controls[i].name)){strcpy(values[i],value);return;}
 assert(0); /* Fails if reset touches video/audio or unrelated settings. */
}
static int Host_WriteConfiguration(void){++saved;return 1;}
static void Sys_Printf(char *s){(void)s;}
'''
main=r'''
int main(void){
 for(int k=0;k<MAX_KEYS;++k){strcpy(normal[k],"unwanted");strcpy(hold[k],"unwanted");strcpy(dt[k],"unwanted");}
 strcpy(normal[100],"+attack");keydown[100]=1;
 cmd_source=1;Wii_ResetControls_f();assert(!saved && keydown[100]);
 cmd_source=0;Wii_ResetControls_f();assert(saved==1 && cleared==1 && released==1 && !in_bind && bindings_currentwatch==-1);
 for(unsigned i=0;i<sizeof(wii_factory_bindings)/sizeof(wii_factory_bindings[0]);++i){
  int k=Key_StringToKeynum((char *)wii_factory_bindings[i].key);
  assert(!strcmp(wii_factory_bindings[i].hold?hold[k]:normal[k],wii_factory_bindings[i].command));
 }
 for(int k=0;k<MAX_KEYS;++k)assert(!dt[k][0]);
 assert(!normal[100][0] && !hold[100][0]);
 assert(!strcmp(normal[Key_StringToKeynum("DPAD_UP")],"impulse 23"));
 assert(!strcmp(hold[Key_StringToKeynum("LEFTFACE")],"+use"));
 for(unsigned i=0;i<sizeof(wii_factory_controls)/sizeof(wii_factory_controls[0]);++i)assert(!strcmp(values[i],wii_factory_controls[i].value));
 Wii_ResetControls_f();assert(saved==2);
 puts("PASS: dirty normal/hold/double-tap bindings reset, old held action released, physical defaults restored, controls-only cvars, saving, remote rejection, idempotence");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-reset-controls-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test'
 p.write_text(stub+'\n'+src+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g',
  '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'source/platform/wii'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
