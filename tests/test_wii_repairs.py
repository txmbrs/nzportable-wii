"""Host regressions of actual resampler, light cache, log ring and binding code."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def read(n):return (root/n).read_text(encoding='utf-8')
def function(s,signature):
 a=s.index(signature);start=s.index('{',a);depth=1;i=start+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[a:i]
preamble='#include <assert.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n#include <limits.h>\n#include <math.h>\n'
image=read('fte-online/fteqw-master/engine/client/image.c')
resample=function(image,'void *Image_ResampleTexture (')
alloc=function(image,'static void *Image_ResampleAlloc(')
formats=list(dict.fromkeys(re.findall(r'case (PTI_\w+):',resample)))
body='typedef enum {'+','.join(formats)+'} uploadfmt_t;\n'
body+='static int fail,calls;static size_t requested;static void *BZF_Malloc(size_t n){requested=n;return fail?NULL:malloc(n);}\n'
for n in ['Image_Resample32Lerp','Image_ResampleTexture32Nearest','Image_ResampleTexture8Nearest','Image_ResampleTexture16Nearest']:
 body+='static void '+n+'(const void *in,int w,int h,void *out,int x,int y){assert(in && out && w>0 && h>0 && x>0 && y>0);++calls;}\n'
body+=alloc+'\n'+resample+r'''
int main(void){
 unsigned char input[128],output[128];memset(input,0x7a,sizeof(input));
 fail=1;assert(!Image_ResampleTexture(PTI_RGBA8,input,4,4,NULL,512,512) && !calls);
 fail=0;void *p=Image_ResampleTexture(PTI_RGBA8,input,4,4,NULL,5,3);assert(p && requested==96 && calls==1);free(p);
 p=Image_ResampleTexture(PTI_L8,input,4,4,NULL,5,3);assert(p && requested==24);free(p);
 p=Image_ResampleTexture(PTI_RGB565,input,4,4,NULL,5,3);assert(p && requested==48);free(p);
 fail=1;assert(Image_ResampleTexture(PTI_RGBA8,input,4,4,output,4,4)==output);
 assert(!Image_ResampleTexture(PTI_RGBA8,input,4,4,NULL,-1,4));
 assert(!Image_ResampleTexture(PTI_RGBA8,input,4,4,NULL,INT_MAX,INT_MAX));
 for(unsigned i=0;i<sizeof(input);++i)assert(input[i]==0x7a);
 puts("PASS: actual image resampler allocation failure, caller buffer, padded sizes, invalid dimensions, original pixels preserved");
}
'''
cases={'resample':body}
light=read('source/render/r_light.c')
a=light.index('#define LIGHTPOINT_CACHE_SIZE');b=light.index('int\nR_LightPoint',a)
body=r'''
#define VectorCopy(a,b) memcpy(b,a,sizeof(float)*3)
typedef float vec3_t[3];typedef struct {int unused;} mplane_t;
typedef struct {void *lightdata,*nodes;} model_t;
static struct {model_t *worldmodel;} cl;static struct {float value;} r_fullbright;
static vec3_t lightcolor,lightspot;static mplane_t *lightplane;
static unsigned lightpoint_generation=1;static int traces,style=10;
static int RecursiveLightPoint(vec3_t color,void *node,vec3_t p,vec3_t end){(void)node;(void)end;++traces;color[0]=style;color[1]=style*2;color[2]=style*3;VectorCopy(p,lightspot);return 1;}
'''+function(light,'void R_InvalidateLightPointCache(')+'\n'+light[a:b]+function(light,'int\nR_LightPoint(')+r'''
int main(void){model_t world={(void*)1,0};cl.worldmodel=&world;vec3_t p={1000,-2000,3000};
 assert(R_LightPoint(p)==20 && R_LightPoint(p)==20 && traces==1);
 style=40;R_InvalidateLightPointCache();assert(R_LightPoint(p)==80 && traces==2);
 R_InvalidateLightPointCache();assert(R_LightPoint(p)==80 && traces==3);
 puts("PASS: actual light cache at large signed coordinates, same-frame reuse, explicit frame/map/style invalidation");}
'''
cases['light']=body
cases['log']=r'''
#include "buffered_log.h"
int main(void){wii_log_buffer_t b={0};char in[4096],out[WII_LOG_CAPACITY],expected[WII_LOG_CAPACITY];
 for(unsigned i=0;i<sizeof(in);++i)in[i]=(char)(i%251);
 for(int i=0;i<20;++i)Wii_LogAppend(&b,in,sizeof(in));
 assert(b.size==WII_LOG_CAPACITY && b.dropped==16384);
 for(int i=0;i<16;++i)memcpy(expected+i*4096,in,4096);
 size_t total=0,n;while((n=Wii_LogTake(&b,out+total,123))!=0)total+=n;
 assert(total==WII_LOG_CAPACITY && !memcmp(out,expected,total));
 assert(!Wii_LogTake(&b,out,sizeof(out)));
 Wii_LogAppend(&b,"abc",3);assert(Wii_LogTake(&b,out,3)==3 && !memcmp(out,"abc",3));
 puts("PASS: bounded memory-only log ring, wrap, overflow accounting, byte order and drain");}
'''
keys=read('source/keys.c');menu=read('source/menu/menu_bindings.c');sys=read('source/menu/menu_sys.c')
body=r'''
#define MAX_KEYS 16
#define PLATFORM_WII 1
#define true 1
#define false 0
#define K_ESCAPE 14
#define K_START 15
#define Q_strlen strlen
#define Q_strcpy strcpy
typedef int qboolean;
static int host_initialized=1,in_bind=1,bindings_currentwatch=2;
static unsigned cvar_archive_revision;
static char *keybindings[MAX_KEYS],*holdbindings[MAX_KEYS],*dtbindings[MAX_KEYS],*current_command,*binding_status="";
static void *Z_Malloc(size_t n){void *p=malloc(n);assert(p);return p;}
static void Z_Free(void *p){free(p);}
static int IN_KeyMatchesActiveDevice(int key){(void)key;return 1;}
'''
for sig in ['void Key_SetBinding (','void Key_SetDTBinding (','void Key_SetHoldBinding (']:body+=function(keys,sig)+'\n'
for sig in ['void Menu_FindKeysForCommand (','void Menu_UnbindCommand (']:body+=function(sys,sig)+'\n'
body+=function(menu,'void Menu_WaitForKeybind (')+r'''
int main(void){
 Key_SetBinding(1,"impulse 23");current_command="+jump";Menu_WaitForKeybind(1);
 assert(in_bind && !strcmp(keybindings[1],"impulse 23"));
 char long_command[200];memset(long_command,'x',199);long_command[199]=0;
 Key_SetBinding(1,long_command);Key_SetHoldBinding(1,"+use");Key_SetDTBinding(1,"target double");
 Key_SetBinding(2,"+jump");Key_SetHoldBinding(2,"source hold");Key_SetDTBinding(2,"source double");
 current_command="+jump";Menu_WaitForKeybind(1);
 assert(!strcmp(keybindings[1],"+jump") && !strcmp(keybindings[2],long_command));
 assert(!strcmp(holdbindings[1],"source hold") && !strcmp(holdbindings[2],"+use"));
 assert(!strcmp(dtbindings[1],"source double") && !strcmp(dtbindings[2],"target double"));
 unsigned revision=cvar_archive_revision;Key_SetBinding(1,keybindings[1]);assert(cvar_archive_revision==revision);
 Key_SetHoldBinding(3,"changed");Key_SetDTBinding(3,"changed");assert(cvar_archive_revision==revision+2);
 Key_SetBinding(4,"impulse 2");Key_SetBinding(5,"impulse 23");int found[2];Menu_FindKeysForCommand("impulse 2",found);assert(found[0]==4 && found[1]==-1);
 current_command="+jump";in_bind=1;Menu_WaitForKeybind(K_ESCAPE);assert(!in_bind && !strcmp(keybindings[1],"+jump"));
 for(int i=0;i<MAX_KEYS;++i){free(keybindings[i]);free(holdbindings[i]);free(dtbindings[i]);}
 puts("PASS: actual binding setters and menu swap, occupied UNBOUND rejection, long commands, hold/double swap, exact matching, cancel and dirty tracking");}
'''
cases['bindings']=body
input_source=read('source/input.c');platform=read('source/platform/wii/in_wii.c')
cases['input_init']=r'''
#define true 1
#define false 0
#define IN_DEVICE_GAMEPAD 1
typedef int qboolean;
static int registered,reset_registered,dump_registered,active;
static struct {float value;} wii_flick_sprint;
static void Cvar_SetValue(const char *name,float value){assert(!strcmp(name,"in_anub_mode") && value==1);}
static void Cvar_RegisterVariable(void *v){assert(v==&wii_flick_sprint);++registered;}
static void Wii_ResetControls_f(void){}
static void Wii_BootFlushLog(void){}
static void Cmd_AddCommand(const char *name,void (*fn)(void)){
 if(!strcmp(name,"wii_reset_controls")){assert(fn==Wii_ResetControls_f);++reset_registered;}
 else {assert(!strcmp(name,"wii_dump_log") && fn==Wii_BootFlushLog);++dump_registered;}
}
static int IN_PlatformHasMouse(void){return 0;}
static void IN_SetActiveDevice(int value){active=value;}
static void IN_SetMouseToRelative(int value){(void)value;assert(0);}
'''+function(platform,'qboolean IN_PlatformHasGamepad(')+'\n'+function(platform,'void IN_PlatformInit(')+'\n'+function(input_source,'void IN_Init(')+r'''
int main(void){IN_Init();assert(registered==1 && reset_registered==1 && dump_registered==1 && active==IN_DEVICE_GAMEPAD);
puts("PASS: actual startup IN_Init reaches Wii cvar/command registration before controller polling");}
'''
with tempfile.TemporaryDirectory(prefix='nzp-repairs-') as tmp:
 for name,body in cases.items():
  p=Path(tmp)/(name+'.c');exe=Path(tmp)/name;p.write_text(preamble+body,encoding='utf-8')
  subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'platform/wii'),str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
