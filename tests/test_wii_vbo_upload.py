"""Compile the real Wii buffer/upload helpers against a checked mock GL backend."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
text=(root/'source/platform/sdl/gl/gl_hyena.c').read_text(encoding='utf-8')
def function(name):
 start=text.index('\n'+name+'(')+1
 start=text.rfind('\n',0,start)+1
 # Include the preceding standalone return-type line.
 start=text.rfind('\n',0,start-1)+1
 opening=text.index('{',start);depth=1;i=opening+1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
stub=r"""
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#define PLATFORM_WII 1
#define true 1
#define false 0
#define HYE_ARRAY_BUFFER 0
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x500
#define GL_INVALID_OPERATION 0x502
#define GL_OUT_OF_MEMORY 0x505
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
typedef unsigned GLenum;
typedef unsigned GLuint;
typedef struct {int target;size_t size;void *data;int owned;GLuint object;} hyena_buffer_t;
static GLenum gl_error;
static int fail_gen,fail_bind,fail_upload,uploads,deletes;
static GLuint bound;
static void *gpu;
static void Sys_Error(const char *message){fprintf(stderr,"%s\n",message);abort();}
static void Sys_Printf(char *format,...){(void)format;}
static GLenum glGetError(void){GLenum e=gl_error;gl_error=0;return e;}
static void glGenBuffers(int n,GLuint *id){assert(n==1);if(fail_gen){*id=0;gl_error=GL_OUT_OF_MEMORY;}else *id=1;}
static void glBindBuffer(GLenum target,GLuint id){assert(target==GL_ARRAY_BUFFER);bound=id;if(id && fail_bind)gl_error=GL_INVALID_OPERATION;}
static void glBufferData(GLenum target,size_t size,const void *data,GLenum usage){
 assert(target==GL_ARRAY_BUFFER && usage==GL_STATIC_DRAW && bound==1);
 ++uploads;if(fail_upload){gl_error=GL_OUT_OF_MEMORY;return;}
 size_t padded=(size+31)&~(size_t)31;
 free(gpu);gpu=malloc(padded);assert(gpu);
 /* Match OpenGX's rounded source read; ASan catches unpadded input. */
 memcpy(gpu,data,padded);
 for(size_t i=size;i<padded;i++)assert(((const unsigned char*)gpu)[i]==0);
}
static void glDeleteBuffers(int n,const GLuint *id){assert(n==1 && *id==1);++deletes;free(gpu);gpu=NULL;}
static void Hyena_DestroyBuffer(hyena_buffer_t *b){if(!b)return;if(b->object)glDeleteBuffers(1,&b->object);if(b->owned)free(b->data);free(b);}
"""
main=r"""
static void reset(void){assert(!gpu);gl_error=0;fail_gen=fail_bind=fail_upload=uploads=deletes=bound=0;}
int main(void){
 const unsigned char bytes[]={1,2,3,4,5,6,7};
 reset();hyena_buffer_t b={0};b.target=HYE_ARRAY_BUFFER;
 Hyena_UpdateBuffer(&b,bytes,sizeof(bytes));gl_error=GL_INVALID_ENUM;
 Hyena_UploadStaticWorldBuffer(&b);
 assert(b.object==1 && !b.owned && !b.data && uploads==1 && !bound);
 assert(!memcmp(gpu,bytes,sizeof(bytes)));glDeleteBuffers(1,&b.object);
 for(int stage=0;stage<3;stage++){
  reset();hyena_buffer_t f={0};f.target=HYE_ARRAY_BUFFER;
  Hyena_UpdateBuffer(&f,bytes,sizeof(bytes));void *original=f.data;
  fail_gen=stage==0;fail_bind=stage==1;fail_upload=stage==2;
  Hyena_UploadStaticWorldBuffer(&f);
  assert(!f.object && f.owned && f.data==original && !bound);
  assert(!memcmp(f.data,bytes,sizeof(bytes)));free(f.data);
 }
 reset();hyena_buffer_t *uv=Hyena_TryCreateStaticArray(bytes,sizeof(bytes));
 assert(uv && uv->object && !uv->data);Hyena_DestroyBuffer(uv);
 reset();fail_upload=1;uv=Hyena_TryCreateStaticArray(bytes,sizeof(bytes));assert(!uv && !gpu);
 reset();hyena_buffer_t empty={0};Hyena_UploadStaticWorldBuffer(&empty);assert(!uploads);
 reset();hyena_buffer_t blank={0};blank.target=HYE_ARRAY_BUFFER;
 Hyena_UpdateBuffer(&blank,NULL,7);for(int i=0;i<32;i++)assert(!((unsigned char*)blank.data)[i]);free(blank.data);
 puts("PASS: stale GL error does not reject valid upload; real generate/bind/upload failures retain fallback; padded uploads and cleanup verified");
}
"""
source=stub+'\n'+function('Hyena_UpdateBuffer')+'\n'+function('Hyena_UploadStaticWorldBuffer')+'\n'+function('Hyena_TryCreateStaticArray')+'\n'+main
with tempfile.TemporaryDirectory(prefix='nzp-wii-vbo-') as tmp:
 path=Path(tmp)/'test.c';binary=Path(tmp)/'test';path.write_text(source,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(path),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
