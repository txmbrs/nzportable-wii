"""Test actual Wii lightmap helpers against the OpenGX packed-pixel reader."""
from pathlib import Path
import re,os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def strip_includes(s):
 return re.sub(r'^#include [<"](?:GL/[^>]+|ogc/[^>]+|[^"\n]+\.h)[>"]\s*$', '', s, flags=re.M)
stream=strip_includes((root/'tools/opengx-pixel_stream.h').read_text(encoding='utf-8'))
texel=strip_includes((root/'tools/opengx-texel.h').read_text(encoding='utf-8'))
tables=(root/'tools/opengx-pixel_stream.cpp').read_text(encoding='utf-8')
tables=tables[tables.index('const struct MasksPerType'):]
helper=(root/'source/platform/wii/lightmap_wii.h').read_text(encoding='utf-8')
backend=(root/'source/platform/sdl/gl/gl_hyena.c').read_text(encoding='utf-8')
a=backend.index('static qboolean Wii_LightmapConverterMatches(')
b=backend.index('\n#endif',a)
functions=backend[a:b]
constants=sorted(set(re.findall(r'\bGL_[A-Z_0-9]+',stream+tables+functions)))
constants+='GX_TF_RGBA8 HYE_TEXTURE_RGBA8 HYE_TEXTURE_INDEX8 HYE_PIXELS_RGBA8 HYE_LINEAR HYE_NEAREST HYE_FILTER_LINEAR HYE_FILTER_NEAREST'.split()
constants=list(dict.fromkeys(constants))
stub='''#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <climits>
typedef unsigned GLenum;typedef int GLint;typedef unsigned GLuint;typedef bool qboolean;
struct GXColor{uint8_t r,g,b,a;};
struct {float transfer_depth_bias=0,transfer_depth_scale=1;int transfer_index_offset=0,transfer_index_shift=0;bool unpack_lsb_first=false;} glparamstate;
'''+''.join('#define '+name+' '+str(1000+i)+'\n' for i,name in enumerate(constants))
mocks=r"""
struct GXTexObj {unsigned format;};
struct cvar_t {float value;};
static cvar_t hyena_lightmap_16bit={1};
struct hyena_texture_t {int width,height,bpp,mipmaps;bool used;};
struct hyena_texture_desc_t {bool lightmap,update;int width,height,update_width,update_height,update_x,update_y,row_stride,format,mip_levels,filter;const void *pixels;};
static hyena_texture_t records[1],*hyena_textures=records;
static GLuint hyena_texture_objects[1]={1};
static int stores[4]={4,0,0,0},uploads,filter_last,converter_mismatch;
static unsigned native_format=GX_TF_RGBA8;
static unsigned char gpu[128*128*4];
static int field(GLenum e){if(e==GL_UNPACK_ALIGNMENT)return 0;if(e==GL_UNPACK_ROW_LENGTH)return 1;if(e==GL_UNPACK_SKIP_ROWS)return 2;assert(e==GL_UNPACK_SKIP_PIXELS);return 3;}
static void glGetIntegerv(GLenum e,GLint *v){*v=stores[field(e)];}
static void glPixelStorei(GLenum e,GLint v){stores[field(e)]=v;}
static bool _ogx_texture_get_texobj(GLuint id,GXTexObj *o){assert(id==1);o->format=native_format;return true;}
static unsigned GX_GetTexObjFmt(GXTexObj *o){return o->format;}
static void Sys_Printf(const char *f,...){(void)f;}
static void _ogx_bytes_to_texture(const void *data,GLenum format,GLenum type,int w,int h,void *out,uint32_t gx,int x,int y,int pitch){
 assert(format==GL_RGBA && gx==GX_TF_RGBA8);
 TexelRGBA8 tex;tex.set_area(out,x,y,w,h,pitch);
 if(type==GL_UNSIGNED_SHORT_5_5_5_1){
  CompoundPixelStream reader(GL_RGBA,type);reader.setup_stream(data,w,h);
  for(int i=0;i<w*h;i++){tex.set_color(reader.read());tex.store();}
  if(converter_mismatch)((uint8_t*)out)[0]^=1;
 }else{assert(type==GL_UNSIGNED_BYTE);const uint8_t *p=(const uint8_t*)data;
  for(int i=0;i<w*h;i++,p+=4){tex.set_color(p[0],p[1],p[2],p[3]);tex.store();}
 }
}
static void Hyena_BackendUploadLevel(int id,int level,int storage,int w,int h,int x,int y,bool replace,const void *data){
 assert(id==0 && !level && storage==HYE_PIXELS_RGBA8 && !replace);
 assert(stores[1]==0 && stores[2]==0 && stores[3]==0);
 _ogx_bytes_to_texture(data,GL_RGBA,GL_UNSIGNED_BYTE,w,h,gpu,GX_TF_RGBA8,x,y,TexelRGBA8::compute_pitch(records[0].width));++uploads;
}
static void Hyena_BackendTextureFilter(int id,int min,int mag){assert(id==0 && min==mag);filter_last=min;}
"""
main=r"""
static uint32_t rng=12345;
static uint8_t random_byte(){rng=rng*1664525u+1013904223u;return rng>>24;}
int main(int argc,char **argv){
 (void)argv;
 if(argc>1){converter_mismatch=1;assert(!Wii_LightmapConverterMatches());assert(!Wii_LightmapConverterMatches());puts("PASS: converter mismatch disables fast path");return 0;}
 stores[0]=8;stores[1]=17;stores[2]=3;stores[3]=5;
 assert(Wii_LightmapConverterMatches());assert(stores[0]==8 && stores[1]==17 && stores[2]==3 && stores[3]==5);
 stores[0]=4;stores[1]=stores[2]=stores[3]=0;
 for(unsigned pixel=0;pixel<65536;pixel++){
  uint8_t packed[2]={(uint8_t)(pixel>>8),(uint8_t)pixel};
  uint8_t src[4]={(uint8_t)(((pixel>>11)&31)<<3),(uint8_t)(((pixel>>6)&31)<<3),(uint8_t)(((pixel>>1)&31)<<3),(uint8_t)((pixel&1)?255:0)},out[4];
  assert(Wii_LightmapRGBA8(out,sizeof(out),src,1,1,4,0,0,1,1,1));
  CompoundPixelStream reader(GL_RGBA,GL_UNSIGNED_SHORT_5_5_5_1);reader.setup_stream(packed,1,1);GXColor ref=reader.read();assert(!memcmp(out,&ref,4));
 }
 uint8_t src[128*520],expected[128*128*4];
 for(auto &v:src)v=random_byte();
 for(int trial=0;trial<1000;trial++){
  int aw=4+random_byte()%125,ah=4+random_byte()%125,stride=aw*4+4;
  int x=random_byte()%aw,y=random_byte()%ah,w=1+random_byte()%(aw-x),h=1+random_byte()%(ah-y);
  int compact=trial&1;
  records[0]={aw,ah,compact?2:4,0,true};hyena_lightmap_16bit.value=compact;
  hyena_texture_desc_t d={true,true,aw,ah,w,h,x,y,stride,HYE_TEXTURE_RGBA8,0,HYE_FILTER_LINEAR,src};
  memset(expected,0x5a,sizeof(expected));memset(gpu,0x5a,sizeof(gpu));
  TexelRGBA8 tex;tex.set_area(expected,x,y,w,h,TexelRGBA8::compute_pitch(aw));
  for(int row=0;row<h;row++)for(int col=0;col<w;col++){
   const uint8_t *p=src+(y+row)*stride+(x+col)*4;GXColor color;
   if(compact){unsigned pixel=((p[0]>>3)<<11)|((p[1]>>3)<<6)|((p[2]>>3)<<1)|(p[3]>>7);uint8_t packed[2]={(uint8_t)(pixel>>8),(uint8_t)pixel};CompoundPixelStream reader(GL_RGBA,GL_UNSIGNED_SHORT_5_5_5_1);reader.setup_stream(packed,1,1);color=reader.read();}
   else memcpy(&color,p,4);
   tex.set_color(color);tex.store();
  }
  assert(Wii_TryFastLightmap(0,&d));assert(!memcmp(expected,gpu,sizeof(gpu)));assert(filter_last==HYE_LINEAR);
  if(trial==0){int before=uploads;d.format=HYE_TEXTURE_INDEX8;assert(!Wii_TryFastLightmap(0,&d));d.format=HYE_TEXTURE_RGBA8;stores[1]=17;assert(!Wii_TryFastLightmap(0,&d));stores[1]=0;native_format=99;assert(!Wii_TryFastLightmap(0,&d));native_format=GX_TF_RGBA8;assert(uploads==before);}
 }
 uint8_t small[4]={};assert(!Wii_LightmapRGBA8(small,3,src,1,1,4,0,0,1,1,1));
 assert(!Wii_LightmapRGBA8(small,4,src,1,1,3,0,0,1,1,1));
 assert(!Wii_LightmapRGBA8(small,4,src,1,1,4,-1,0,1,1,1));
 assert(!Wii_LightmapRGBA8(small,4,src,1,1,4,0,0,0,1,1));
 records[0]={128,128,4,0,true};hyena_lightmap_16bit.value=0;
 hyena_texture_desc_t full={true,true,128,128,0,0,0,0,512,HYE_TEXTURE_RGBA8,0,HYE_FILTER_NEAREST,src};
 assert(Wii_TryFastLightmap(0,&full));assert(filter_last==HYE_NEAREST);
 puts("PASS: 65536 packed colors, 1000 strided/tiled updates, full precision, pixel-store state, filtering, bounds and fallback guards");
}
"""
with tempfile.TemporaryDirectory(prefix='nzp-lightmap-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test'
 src.write_text(stub+'\n'+stream+'\n'+tables+'\n'+texel+'\n'+helper+'\n'+mocks+'\n'+functions+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 subprocess.run([str(exe),'mismatch'],check=True)
