"""Actual Sys_Printf: runtime logs survive without hidden stdout rendering."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'fte-online/fteqw-master/engine/client/sys_sdl.c').read_text(encoding='utf-8')
a=s.index('void Sys_Printf (');b=s.index('\n}\n',a)+2;body=s[a:b]
stub=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#define PLATFORM_WII 1
#define false 0
#define CON_WHITEMASK 0
#define CON_HIDDEN 1
#define CON_NONCLEARBG 2
typedef unsigned conchar_t;
static int runtime,logs,parsed,colours;
static char logged[2048];
static unsigned Sys_Milliseconds(void){return 100;}
static void WiiOnline_Log(const char *text){++logs;snprintf(logged,sizeof(logged),"%s",text);}
static int WiiOnline_RuntimeLogActive(void){return runtime;}
static void Sys_Error(const char *text){(void)text;abort();}
static conchar_t *COM_ParseFunString(int flags,const char *text,conchar_t *out,size_t size,int mode){
 (void)flags;(void)mode;++parsed;size_t n=strlen(text);assert(n<size/sizeof(*out));
 for(size_t i=0;i<n;++i)out[i]=(unsigned char)text[i];return out+n;
}
static conchar_t *Font_Decode(conchar_t *c,unsigned *flags,unsigned *point){*flags=0;*point=*c;return c+1;}
static void ApplyColour(unsigned flags){(void)flags;++colours;}
'''
main=r'''
int main(void){
 runtime=1;for(int i=0;i<1000;++i)Sys_Printf("message %d",i);
 assert(logs==1000 && parsed==0 && colours==0 && !strcmp(logged,"message 999"));
 runtime=0;Sys_Printf("startup/fatal path\n");assert(logs==1001 && parsed==1 && colours>0);
 puts("PASS: 1000 runtime messages logged without stdout conversion; startup/fatal console output retained");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-output-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test';p.write_text(stub+'\n'+body+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
