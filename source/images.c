/*
NZ:P Universal Image loading
Copyright (C) 2025

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "nzportable_def.h"

int		    image_width;
int		    image_height;

/*
============
tex_filebase
============
*/
// create a unique identifier for each externally loaded texture based on filename
void tex_filebase (char *in, char *out)
{
	if (in == NULL || !*in) return;

	char *s, *s2;
	
	s = in + strlen(in) - 1;
	
	for (s2 = s ; s2 != in && *s2 && *s2 != '/' ; s2--) {
		;
	}
	
	if (s-s2 < 2) {
		Sys_Error("tex_filebase: bad input\n");
	} else {
		strncpy (out,s2+1, (s+1)-(s2));
		out[s-s2] = 0;
	}
}

/*
=================================================================
  PCX Loading
  mem fix by Crow_bar.
=================================================================
*/
typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

/* PCX stores 16-bit words little-endian; do not depend on platform macros. */
static int PCX_ReadHeader(FILE *file,pcx_t *pcx)
{
    unsigned char header[128];
    if(!file||fread(header,1,sizeof(header),file)!=sizeof(header))return 0;
    memset(pcx,0,sizeof(*pcx));
    pcx->manufacturer=header[0];pcx->version=header[1];pcx->encoding=header[2];pcx->bits_per_pixel=header[3];
#define PCX_WORD(offset) ((unsigned short)(header[offset]|((unsigned)header[(offset)+1]<<8)))
    pcx->xmin=PCX_WORD(4);pcx->ymin=PCX_WORD(6);pcx->xmax=PCX_WORD(8);pcx->ymax=PCX_WORD(10);
    pcx->hres=PCX_WORD(12);pcx->vres=PCX_WORD(14);
    pcx->reserved=header[64];pcx->color_planes=header[65];
    pcx->bytes_per_line=PCX_WORD(66);pcx->palette_type=PCX_WORD(68);
#undef PCX_WORD
    memcpy(pcx->palette,header+16,48);memcpy(pcx->filler,header+70,58);
    return 1;
}

static int PCX_ReadPixels(FILE *file,const pcx_t *pcx,byte *palette,byte *pixels,int channels)
{
    int width=pcx->xmax-pcx->xmin+1,height=pcx->ymax-pcx->ymin+1;
    if(fseek(file,0,SEEK_END))return 0;
    long length=ftell(file),data_end=length-769;
    if(data_end<128||fseek(file,data_end,SEEK_SET)||fgetc(file)!=12||fread(palette,1,768,file)!=768||fseek(file,128,SEEK_SET))return 0;
    long remaining=data_end-128;
    for(int y=0;y<height;y++)for(int x=0;x<pcx->bytes_per_line;){
        if(remaining--<=0)return 0;
        int value=fgetc(file),run=1;if(value==EOF)return 0;
        if((value&0xc0)==0xc0){run=value&63;if(remaining--<=0)return 0;value=fgetc(file);}
        if(value==EOF||!run||run>pcx->bytes_per_line-x)return 0;
        while(run--){
            if(x<width){
                if(channels==1)*pixels++=(byte)value;
                else {memcpy(pixels,palette+value*3,3);pixels[3]=255;pixels+=4;}
            }
            x++;
        }
    }
    return 1;
}

byte* LoadPCX(FILE* f, int matchwidth, int matchheight) 
{
    if(!f)return NULL;
    pcx_t pcxbuf = {0};
    if (!PCX_ReadHeader(f,&pcxbuf)) {
        fclose(f);
        Sys_Error("Truncated pcx header\n");
        return NULL;
    }

    pcx_t* pcx = &pcxbuf;

    if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
        pcx->bits_per_pixel != 8 || pcx->color_planes!=1 || pcx->xmax<pcx->xmin || pcx->ymax<pcx->ymin ||
        pcx->xmax-pcx->xmin>=640 || pcx->ymax-pcx->ymin>=480 || pcx->bytes_per_line<pcx->xmax-pcx->xmin+1) {
		fclose(f);
        Sys_Error("Bad pcx header: version=%d bpp=%d planes=%d bounds=%u,%u..%u,%u stride=%u\n",
            pcx->version,pcx->bits_per_pixel,pcx->color_planes,pcx->xmin,pcx->ymin,pcx->xmax,pcx->ymax,pcx->bytes_per_line);
        return NULL;
    }

    if (matchwidth && (pcx->xmax-pcx->xmin + 1) != matchwidth) {
		Sys_Error("Matchwidth!\n");
        fclose(f);
        return NULL;
    }

    if (matchheight && (pcx->ymax-pcx->ymin + 1) != matchheight) {
		Sys_Error("Matchheight!\n");
        fclose(f);
        return NULL;
    }

    unsigned char palette[768];
    int width=pcx->xmax-pcx->xmin+1,height=pcx->ymax-pcx->ymin+1;
    byte *image_rgba=(byte *)Q_malloc(4*width*height);
    if(!PCX_ReadPixels(f,pcx,palette,image_rgba,4)){
        free(image_rgba);fclose(f);Sys_Error("Invalid or truncated PCX palette/RLE data\n");return NULL;
    }
    image_width=width;image_height=height;

    fclose(f);
    return image_rgba;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** */
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
byte* LoadSTBI_Image(FILE *f)
{
	int bpp;
	int inwidth, inheight;
	byte* image = stbi_load_from_file(f, &inwidth, &inheight, &bpp, 4);

	if(image == NULL) {
		Sys_Error(" stbi failure: %s\n", stbi_failure_reason());
		return NULL;
	}

	image_width = inwidth;
	image_height = inheight;
	fclose(f);
	return image;
}

byte* Image_LoadPixels(char* filename, int image_format)
{
	FILE	*f;
	char name[MAX_QPATH];

	if (image_format & IMAGE_PCX) {
		snprintf (name, sizeof(name), "%s.pcx", filename);
		if (COM_FOpenFile(name, &f) != -1 && f) {
			Sys_Printf("Loading PCX: %s\n",name);
			return LoadPCX (f, 0, 0);
        }
	}

	if (image_format & IMAGE_TGA) {
		snprintf (name, sizeof(name), "%s.tga", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_PNG) {
		snprintf (name, sizeof(name), "%s.png", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_JPG) {
		snprintf (name, sizeof(name), "%s.jpg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);

		snprintf (name, sizeof(name), "%s.jpeg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	return NULL;					
}

image_t Image_LoadImageWithIdentifier(char *filename, char *identifier, int image_format,
  int filter, bool keep, bool mipmap)
{
	int texture_index;
	byte *data;

	if (filename == NULL || identifier == NULL || identifier[0] == '\0'
	  || strlen(identifier) >= 64) {
		Con_DPrintf("bad texture %s\n", filename);
		return -1;
	}

	// does the texture already exist?
	texture_index = Hyena_FindTexture(identifier);
	if (texture_index >= 0) {
		return texture_index;
	}

	data = Image_LoadPixels (filename, image_format);

	if(data == NULL) {
		return -1;
	}

	texture_index = Image_LoadTexture(identifier, image_width, image_height, data, HYE_TEXTURE_RGBA8, filter, mipmap ? 1 : 0, true, keep, true);

	if(texture_index < 0) {
		Sys_Error("Image_LoadImage: failed to upload texture %s\n", identifier);
	}

	free(data);

	return texture_index;
}

image_t Image_LoadImage(char* filename, int image_format, int filter, bool keep, bool mipmap)
{
	char texname[64] = {0};

	if (filename == NULL) return -1;
	tex_filebase (filename, texname);
	return Image_LoadImageWithIdentifier(filename, texname, image_format, filter, keep, mipmap);
}

// used on PSP
#ifdef __PSP__
/*
=============
loadrgbafrompal
=============
*/
int loadrgbafrompal (char* name, int width, int height, byte* data)
{
	int pixels = width * height;
	byte* rgbadata = (byte*)Q_malloc(pixels * 4);
	char texname[32];

	tex_filebase (name, texname);

	for(int i = 0; i < pixels; i++) {
		byte palette_index = data[i];
		rgbadata[i * 4]     = host_basepal[palette_index * 3 + 0];
        rgbadata[i * 4 + 1] = host_basepal[palette_index * 3 + 1];
        rgbadata[i * 4 + 2] = host_basepal[palette_index * 3 + 2];
        rgbadata[i * 4 + 3] = 255; // Set alpha to opaque
	}

	int ret = Image_LoadTexture(texname, width, height, rgbadata, HYE_TEXTURE_RGBA8, HYE_FILTER_LINEAR, 0, true, true, true);

	free(rgbadata);
	return ret;
}

/*
=============
loadpcxas4bpp
=============
converts an indexed pcx to a 16 color one, if there's a palhint file, use that palette
*/
int loadpcxas4bpp (char* filename, int filter) 
{
	FILE* f;
	char name[128];
	char texname[32];

	sprintf(name, "%s.pcx", filename);
	tex_filebase (name, texname);
	COM_FOpenFile(name, &f);
	if (!f) {
		Con_DPrintf("Could not load PCX file %s\n", name);
		return -1;
	}

    pcx_t pcxbuf;
    if(!PCX_ReadHeader(f,&pcxbuf)){fclose(f);return -1;}
    pcx_t *pcx=&pcxbuf;
    if(pcx->manufacturer!=10||pcx->version!=5||pcx->encoding!=1||pcx->bits_per_pixel!=8||pcx->color_planes!=1||
       pcx->xmax<pcx->xmin||pcx->ymax<pcx->ymin||pcx->xmax-pcx->xmin>=320||pcx->ymax-pcx->ymin>=256||
       pcx->bytes_per_line<pcx->xmax-pcx->xmin+1){
        fclose(f);Con_DPrintf("Bad pcx file %s\n",name);return -1;
    }
    unsigned int width=pcx->xmax-pcx->xmin+1,height=pcx->ymax-pcx->ymin+1;
    byte palette[768];byte *data=(byte *)Q_malloc(width*height);
    if(!PCX_ReadPixels(f,pcx,palette,data,1)){free(data);fclose(f);return -1;}
    fclose(f);

	int texture = -1;
	sprintf(name, "%s.palhint", filename);
	FILE* f2;
	COM_FOpenFile(name, &f2);

	if (!f2) {
		texture = Hyena_LoadPalettedTexture(texname, width, height, data, palette, 3, NULL, filter, 0, true, true);
	} else {
		// contain padding for extra whitespace etc 
		size_t size = 16 * 4 * 2 * 2;
		char filecontent[size];
		int bytes = fread(&filecontent, 1, size, f2);
		fclose(f2);

		int index = 0;
		unsigned int palhint[16];
		memset(palhint, 0xff, 16 * 4);
		int colors_read = 0;
		char color[7];
		while (index < (bytes - 1)) {
			if (filecontent[index] == '#') {
				index++;
				memcpy(color, &(filecontent[index]), 6);
				color[6] = '\0';
				int parsed = strtol(color, NULL, 16);
				byte r, g, b;
				r = parsed >> 16;
				g = parsed >> 8;
				b = parsed;
				palhint[colors_read] = 0xff000000 + (b << 16) + (g << 8) + r;
				colors_read++;
				index += 6;
			} else {
				index++;
			}
		}
		texture = Hyena_LoadPalettedTexture(texname, width, height, data, palette, 3, (byte *)palhint, filter, 0, true, true);
	}

	free(data);

	return texture;
}
#endif

byte findclosestpalmatch(byte r, byte g, byte b, byte a)
{
	int best_distance = 0x7fffffff;
	int best_index = 0;
	int i;

	if (a < 128)
		return 255;
	for (i = 0; i < 256; ++i) {
		int red = r - host_basepal[i * 3];
		int green = g - host_basepal[i * 3 + 1];
		int blue = b - host_basepal[i * 3 + 2];
		int distance = red * red + green * green + blue * blue;
		if (distance < best_distance) {
			best_distance = distance;
			best_index = i;
		}
	}
	return (byte)best_index;
}

int
Image_LoadTexture(const char *identifier, int width, int height, const void *pixels,
    hyena_texture_format_t format, hyena_texture_filter_t filter, int mip_levels,
    qboolean alpha, qboolean keep, qboolean stretch_to_power_of_two)
{
    hyena_texture_desc_t desc = { 0 };
    size_t length = strlen(identifier);

    desc.identifier = identifier;
    desc.pixels = pixels;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.filter = filter;
    desc.mip_levels = mip_levels;
    desc.alpha = alpha;
    desc.keep = keep;
    desc.stretch_to_power_of_two = stretch_to_power_of_two;
    // Effect images and sprite frames need interpolated alpha even with DXT1 selected.
    desc.smooth_alpha = (length >= 7 && identifier[length - 7] == '$') || (length >= 5 && !strncmp(identifier + length - 5, "spr", 3));
    return Hyena_CreateTexture(&desc);
}
