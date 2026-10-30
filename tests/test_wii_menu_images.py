"""Exercise the actual Wii menu picture cache with mocked texture/GX APIs."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/platform/wii/menu_images_wii.c').read_text(encoding='utf-8')
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
stub=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define IMAGE_PNG 1
#define IMAGE_TGA 2
#define IMAGE_JPG 4
#define false 0
static struct {int map_use_thumbnail;char *map_thumbnail_path;} custom_maps[100];
static int num_user_maps=100,menu_background=99,menu_bk=99;
static float menu_changetime=10;
static struct {char name[64];int live;} registry[32];
static int loads,fences,invalidates,deletes,live,peak,missing;
static int Hyena_FindTexture(char *id){for(int i=0;i<32;++i)if(registry[i].live && !strcmp(registry[i].name,id))return i;return -1;}
static void GX_DrawDone(void){++fences;}
static void GX_InvalidateTexAll(void){++invalidates;}
static void Hyena_DestroyTexture(int i){assert(i>=0 && i<32 && registry[i].live);assert(fences==deletes+1);registry[i].live=0;--live;++deletes;}
static int Image_LoadImageWithIdentifier(char *path,char *id,int formats,int filter,int keep,int mip){
 ++loads;assert(formats==7 && filter==1 && !keep && !mip);
 if(!strcmp(path,"missing")){++missing;return -1;}
 assert(Hyena_FindTexture(id)<0);
 for(int i=0;i<32;++i)if(!registry[i].live){registry[i].live=1;strcpy(registry[i].name,id);++live;if(live>peak)peak=live;assert(live<=2);return i;}
 assert(0);return -1;
}
'''
main=r'''
int main(void){
 for(int i=0;i<100;++i){custom_maps[i].map_use_thumbnail=1;custom_maps[i].map_thumbnail_path="picture";}
 Wii_MenuImagesReset();assert(!loads && !live && menu_background==99 && !menu_changetime);
 int bg=Wii_MenuMapImage(0,1);assert(bg>=0 && loads==1);
 int pic=Wii_MenuMapImage(1,0);assert(pic>=0 && loads==2 && pic!=bg);
 for(int i=0;i<1000;++i){assert(Wii_MenuMapImage(0,1)==bg);assert(Wii_MenuMapImage(1,0)==pic);}
 assert(loads==2);
 for(int i=0;i<1000;++i){assert(Wii_MenuMapImage(i%100,0)>=0);assert(Hyena_FindTexture("wii_menu_background")==bg);}
 assert(peak==2 && live==2 && fences==deletes && invalidates==deletes);
 for(int i=0;i<100;++i)assert(Wii_MenuMapImage(i,1)>=0);
 assert(live==2);
 /* Map load destroys registry objects. Their IDs can be reused by game data. */
 for(int i=0;i<32;++i)registry[i].live=0;live=0;
 int reused=slots[1].texture;strcpy(registry[reused].name,"game_texture");registry[reused].live=1;
 Wii_MenuImagesReset();assert(registry[reused].live && !strcmp(registry[reused].name,"game_texture"));
 registry[reused].live=0;
 assert(Wii_MenuMapImage(99,1)>=0);
 custom_maps[3].map_thumbnail_path="missing";
 assert(Wii_MenuMapImage(3,0)==-1);int before=loads;
 for(int i=0;i<1000;++i)assert(Wii_MenuMapImage(3,0)==-1);
 assert(loads==before && missing==1);
 assert(Wii_MenuMapImage(-1,0)==-1 && Wii_MenuMapImage(100,0)==-1);
 custom_maps[4].map_use_thumbnail=0;assert(Wii_MenuMapImage(4,0)==-1);
 Wii_MenuImagesReset();assert(!live);assert(fences==deletes && invalidates==deletes);
 puts("PASS: bounded two-picture residency, repeated draw reuse, 1000 preview changes, background rotation, GPU fences, stale ID protection, missing files and reset");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-menu-images-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test'
 p.write_text(stub+'\n'+source+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g',
  '-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
