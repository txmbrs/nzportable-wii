"""Host regression test of the actual r_mdl.c Wii batching/cache logic.
No engine, graphics context or game is launched. Requires a C compiler.
"""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
stub=r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define PLATFORM_WII 1
#define true 1
#define false 0
typedef int qboolean;
typedef unsigned char byte;
typedef struct { byte v[3], normal; } trivertx_t;
typedef struct { int id; } model_t;
typedef struct { model_t *model; } entity_t;
entity_t *currententity;
typedef struct hyena_buffer_s { void *data; size_t size; int owned,immutable; } hyena_buffer_t;
typedef struct {hyena_buffer_t *buffer; const void *data; int type,stride;} hyena_array_t;
typedef struct {hyena_array_t position,uv,color; int count; float scale; unsigned flags;} hyena_arrays_t;
enum {HYE_TRIANGLES,HYE_TRIANGLE_FAN,HYE_TRIANGLE_STRIP,HYE_PACKED_UV_SHORT=8,HYE_PACKED_XYZ_BYTE=16};
enum {HYE_SHORT=1,HYE_ARRAY_BUFFER=0,HYE_CLIENT_INDEX_BUFFER=2};
static int live_buffers,uv_attempts,uv_fail,was_hit,draws;
static const trivertx_t *expect1,*expect2;
static float expect_blend;
static int expect_count;
static void Sys_Error(const char *s) { fprintf(stderr,"%s\n",s); abort(); }
static int Hyena_PackedArraysSupported(void) {return 0;}
static void *Hyena_AllocateTransient(size_t n) {return malloc(n);}
static void Hyena_MorphWeight(int i,float f) {(void)i;(void)f;}
static void Hyena_DrawPacked(int a,int b,int c,int d,const void *e,const unsigned short *f,unsigned g)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;assert(0);}
static size_t Hyena_StaticVertexBudget(void) {return 0;}
static hyena_buffer_t *Hyena_CreateBuffer(int target,const void *data,size_t size) {
 hyena_buffer_t *b=calloc(1,sizeof(*b));assert(b);++live_buffers;
 b->size=size; b->owned=target==HYE_ARRAY_BUFFER;
 if(b->owned && size) {b->data=malloc(size);assert(b->data);memcpy(b->data,data,size);}
 else b->data=(void*)data;
 return b;
}
static void Hyena_DestroyBuffer(hyena_buffer_t *b) {
 if(!b)return;if(b->owned)free(b->data);free(b);--live_buffers;
}
static void Hyena_UpdateBuffer(hyena_buffer_t *b,const void *data,size_t size) {
 assert(!b->immutable && !b->owned); b->data=(void*)data;b->size=size;
}
static hyena_buffer_t *Hyena_TryCreateStaticArray(const void *data,size_t size) {
 ++uv_attempts;if(uv_fail)return NULL;
 hyena_buffer_t *b=Hyena_CreateBuffer(HYE_ARRAY_BUFFER,data,size);b->immutable=1;return b;
}
static double Wii_PerfClock(void) {return 1;}
static void Wii_PerfAlias(double a,double b,int hit,int vertices) {(void)a;(void)b;(void)vertices;was_hit=hit;}
static void Hyena_DrawElements(int mode,const hyena_arrays_t *a,int count,hyena_buffer_t *indices,size_t offset) {
 (void)mode;assert(a->position.type==HYE_SHORT && a->scale==1.0f/128.0f);
 const short *pos=a->position.data;
 const float *uv=a->uv.buffer?a->uv.buffer->data:a->uv.data;
 const unsigned short *ix=(const void*)((const byte*)indices->data+offset);
 assert(a->count==expect_count && count==3*(expect_count-2));
 for(int i=0;i<expect_count;i++) {
  for(int j=0;j<3;j++) {
   short ref=expect2?(short)((expect1[i].v[j]+expect_blend*(expect2[i].v[j]-expect1[i].v[j]))*128.0f):expect1[i].v[j]*128;
   assert(pos[i*3+j]==ref);
  }
  assert(uv[i*2]==(float)i/expect_count && uv[i*2+1]==0.5f);
 }
 for(int i=0;i<count;i++)assert(ix[i]<expect_count);
 if(expect_count==4){const unsigned short ref[]={0,1,2,2,1,3};assert(!memcmp(ix,ref,sizeof(ref)));}
 ++draws;
}
"""
engine=(root/'source/render/r_mdl.c').read_text(encoding='utf-8')
engine=engine.replace('#include "../nzportable_def.h"','').replace('#include "../platform/wii/perf_wii.h"','')
header=(root/'source/render/r_mdl.h').read_text(encoding='utf-8')
main=r"""
static int *commands(int n) {
 int *c=calloc(2*n+2,sizeof(*c));assert(c);c[0]=n;
 for(int i=0;i<n;i++){float uv[2]={(float)i/n,0.5f};memcpy(c+1+i*2,uv,sizeof(uv));}return c;
}
static void run(const int *c,const trivertx_t *a,const trivertx_t *b,float blend,int n) {
 expect1=a;expect2=b;expect_blend=blend;expect_count=n;R_DrawAliasCommands(c,a,b,blend,0,0);
}
static void clear(void) {
 R_ClearAliasTopologyCache();assert(!live_buffers && !wii_alias_uv_bytes && !wii_alias_pose_bytes && !wii_alias_uv_objects);
 assert(!r_alias_positions && !r_alias_vertex_capacity);
}
int main(void) {
 model_t models[40];entity_t entity={&models[0]};currententity=&entity;
 trivertx_t poses[8][4];uint32_t random=123;
 for(int i=0;i<8;i++)for(int j=0;j<4;j++)for(int k=0;k<3;k++){random=random*1664525u+1013904223u;poses[i][j].v[k]=random>>24;}
 int *c=commands(4);
 for(int i=0;i<1000;i++) {
  float blend=(i%101)/100.0f;const trivertx_t *a=poses[i%8],*b=poses[(i+3)%8];
  run(c,a,b,blend,4);run(c,a,b,blend,4);assert(was_hit);
 }
 assert(uv_attempts==1);
 run(c,poses[0],NULL,0,4);run(c,poses[0],NULL,1,4);assert(was_hit);
 int *relocated=commands(4);run(relocated,poses[1],poses[2],0.25f,4);assert(uv_attempts==2 && wii_alias_uv_objects==1);
 free(c);free(relocated);clear();
 c=commands(4);uv_fail=1;int before=uv_attempts;
 run(c,poses[0],poses[1],0.3f,4);run(c,poses[0],poses[1],0.3f,4);
 assert(uv_attempts==before+1);clear();uv_fail=0;
 for(int i=0;i<40;i++){entity.model=&models[i];run(c,poses[0],NULL,0,4);}
 assert(wii_alias_uv_objects==32 && wii_alias_uv_bytes<=WII_ALIAS_UV_BUDGET);clear();free(c);
 int n=16384;trivertx_t *large=calloc(n,sizeof(*large));int *big=commands(n);
 entity.model=&models[0];run(big,large,NULL,0,n);assert(r_alias_topologies->cached_positions);
 entity.model=&models[1];run(big,large,NULL,0,n);assert(!r_alias_topologies->cached_positions);
 run(big,large,NULL,0,n);assert(!was_hit);
 assert(wii_alias_pose_bytes<=WII_ALIAS_POSE_BUDGET);clear();free(big);free(large);
 printf("PASS: %d draws, exact interpolation, cache hits, UV immutability, allocation fallback, object/memory limits, relocation and teardown\n",draws);
}
"""
with tempfile.TemporaryDirectory(prefix='nzp-alias-cache-') as tmp:
 source=Path(tmp)/'test.c';binary=Path(tmp)/'test'
 source.write_text(stub+'\n'+header+'\n'+engine+'\n'+main,encoding='utf-8')
 cmd=[os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(source),'-o',str(binary)]
 subprocess.run(cmd,check=True)
 subprocess.run([str(binary)],check=True)
