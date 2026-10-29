#include "../../../nzportable_def.h"
#ifdef PLATFORM_WII
#include "../../../platform/wii/perf_wii.h"
#endif
#include "../sdl_local.h"
#ifdef PLATFORM_WII
#include <gccore.h>
#include <ogc/conf.h>
#include <wiiuse/wpad.h>
#include "../../wii/boot_wii.h"
#endif

SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;
int sdl_window_width = 640;
int sdl_window_height = 480;
cvar_t vid_width = {"vid_width", "640", true};
cvar_t vid_height = {"vid_height", "480", true};
cvar_t vid_fullscreen = {"vid_fullscreen", "0", true};
cvar_t r_vsync = {"r_vsync", "1", true};

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];
int texture_mode = GL_LINEAR;
double gldepthmin, gldepthmax;
cvar_t gl_ztrick = {"gl_ztrick", "0"};
qboolean isPermedia = true;
static float vid_gamma = 1.0f;
static int sdl_fullscreen = -1;
static int sdl_vsync = -1;
#ifdef PLATFORM_WII
/* -1 follows the console setting; 0/1 explicitly selects 4:3/16:9. */
static cvar_t wii_widescreen = {"wii_widescreen", "-1", true};
cvar_t wii_image_smoothing = {"wii_image_smoothing", "1", CVAR_ARCHIVE};
static GXRModeObj wii_video_mode;
static qboolean wii_video_mode_valid;

static void VID_WiiPresentation(void)
{
    SDL_DisplayMode display;
    GXRModeObj mode;
    int index = SDL_GetWindowDisplayIndex(sdl_window);
    if (index < 0 || SDL_GetCurrentDisplayMode(index, &display) != 0 || !display.driverdata) {
        Con_Printf("Wii: could not read active SDL video mode; keeping VI settings.\n");
        return;
    }
    /* SDL's OGC backend stores the active GXRModeObj in driverdata.
     * Stretch the XFB in the video interface, not the GX render target.
     * Keep the selected TV standard, interlace mode and framebuffer sizes. */
    mode = *(const GXRModeObj *)display.driverdata;
    mode.viWidth = 720;
    mode.viXOrigin = 0;
    wii_video_mode = mode;
    wii_video_mode_valid = true;
    VIDEO_Configure(&mode);
    VIDEO_Flush();
    Sys_Printf("Wii video: EFB %dx%d, VI width %d, console aspect %s\n",
      mode.fbWidth, mode.efbHeight, mode.viWidth,
      CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? "16:9" : "4:3");
}
#endif

void GL_Init(void)
{
	glClearDepth(1.0);
	glClearColor(16.0f / 255.0f, 32.0f / 255.0f, 64.0f / 255.0f, 1.0f);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	VID_SDLResize();
	*x = *y = 0;
	SDL_GL_GetDrawableSize(sdl_window, width, height);
	sdl_window_width = *width;
	sdl_window_height = *height;
}

void VID_SDLResize(void)
{
	int width, height, logical_width;
	if (!sdl_window) return;
	SDL_GL_GetDrawableSize(sdl_window, &width, &height);
	if (width < 1 || height < 1) return;
	logical_width = width;
#ifdef PLATFORM_WII
    /* Anamorphic widescreen: the camera and HUD use a 16:9 logical canvas,
     * while GX continues rendering the physical 640-pixel framebuffer. */
    if (wii_widescreen.value < 0 ? CONF_GetAspectRatio() == CONF_ASPECT_16_9 : wii_widescreen.value != 0)
        logical_width = (height * 16 + 4) / 9;
    else
        logical_width = (height * 4 + 1) / 3;
#endif
	if (logical_width == (int)vid.width && height == (int)vid.height &&
	    width == sdl_window_width && height == sdl_window_height) return;
	sdl_window_width = width;
	sdl_window_height = height;
	vid.width = vid.maxwarpwidth = logical_width;
	vid.height = vid.maxwarpheight = height;
	vid.aspect = ((float)height / logical_width) * (320.0f / 240.0f);
	vid.scale = height / STD_UI_HEIGHT;
	if (vid.scale < 1) vid.scale = 1;
	vid.conwidth = logical_width;
	vid.conheight = height;
	vid.recalc_refdef = true;
}

void VID_SetFullscreen(qboolean fullscreen)
{
	if (!sdl_window) return;
	if (SDL_SetWindowFullscreen(sdl_window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
		Con_Printf("Could not change fullscreen mode: %s\n", SDL_GetError());
		return;
	}
	Cvar_SetValue("vid_fullscreen", fullscreen ? 1 : 0);
	sdl_fullscreen = fullscreen ? 1 : 0;
#ifdef PLATFORM_WII
    VID_WiiPresentation();
#endif
	VID_SDLResize();
}

void VID_SetVSync(qboolean vsync)
{
	if (!sdl_gl_context) return;
	if (SDL_GL_SetSwapInterval(vsync ? 1 : 0) != 0) {
		Con_Printf("Could not change VSync: %s\n", SDL_GetError());
		return;
	}
	Cvar_SetValue("r_vsync", vsync ? 1 : 0);
	sdl_vsync = vsync ? 1 : 0;
}

void GL_EndRendering(void)
{
	int fullscreen = vid_fullscreen.value != 0.0f;
	int vsync = r_vsync.value != 0.0f;
	Draw_Flush();
	if (fullscreen != sdl_fullscreen)
		VID_SetFullscreen(fullscreen);
	if (vsync != sdl_vsync)
		VID_SetVSync(vsync);
#ifdef PLATFORM_WII
    if (wii_video_mode_valid) {
        /* A mild three-line copy filter, with no extra scene pass or
         * reduction of the 480-line render target. Coefficients sum to 64. */
        static const u8 low_filter[7] = {0, 0, 8, 48, 8, 0, 0};
        GX_SetCopyFilter(wii_video_mode.aa, wii_video_mode.sample_pattern,
          wii_image_smoothing.value != 0, low_filter);
    }
#endif
#ifdef PLATFORM_WII
    double present_start = Wii_PerfClock();
#endif
	SDL_GL_SwapWindow(sdl_window);
#ifdef PLATFORM_WII
    Wii_PerfAdd(WII_PERF_PRESENT,present_start);
#endif
}

void VID_SetPalette(unsigned char *palette)
{
	unsigned i;
	for (i = 0; i < 256; ++i)
		d_8to24table[i] = (255u << 24) | palette[i * 3] | (palette[i * 3 + 1] << 8) | (palette[i * 3 + 2] << 16);
	d_8to24table[255] &= 0x00ffffff;
#ifdef PLATFORM_WII
    for (i = 0; i < 256; ++i) d_8to24table[i] = __builtin_bswap32(d_8to24table[i]);
#endif
}

void VID_ShiftPalette(unsigned char *palette) { (void)palette; }

static void Check_Gamma(unsigned char *pal)
{
	unsigned char adjusted[768];
	int i, parameter = COM_CheckParm("-gamma");
	vid_gamma = parameter && parameter + 1 < com_argc ? Q_atof(com_argv[parameter + 1]) : 0.7f;
	for (i = 0; i < 768; ++i) {
		float value = powf((pal[i] + 1) / 256.0f, vid_gamma) * 255.0f + 0.5f;
		adjusted[i] = (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
	}
	memcpy(pal, adjusted, sizeof(adjusted));
}

void VID_Init(unsigned char *palette)
{
	int parameter;
	Cvar_RegisterVariable(&vid_width);
	Cvar_RegisterVariable(&vid_height);
	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&r_vsync);
#ifdef PLATFORM_WII
    Cvar_RegisterVariable(&wii_widescreen);
    Cvar_RegisterVariable(&wii_image_smoothing);
#endif
	sdl_window_width = (int)vid_width.value;
	sdl_window_height = (int)vid_height.value;
	parameter = COM_CheckParm("-width");
	if (!parameter) parameter = COM_CheckParm("+vid_width");
	if (parameter && parameter + 1 < com_argc) sdl_window_width = Q_atoi(com_argv[parameter + 1]);
	parameter = COM_CheckParm("-height");
	if (!parameter) parameter = COM_CheckParm("+vid_height");
	if (parameter && parameter + 1 < com_argc) sdl_window_height = Q_atoi(com_argv[parameter + 1]);
#ifndef PLATFORM_WII
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif
	sdl_window = SDL_CreateWindow("Nazi Zombies: Portable", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		sdl_window_width, sdl_window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
        (vid_fullscreen.value ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
#ifdef PLATFORM_WII
    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(0, 640, 480);
    SDL_ShowCursor(SDL_DISABLE);
#endif
	if (!sdl_window) Sys_Error("SDL_CreateWindow: %s", SDL_GetError());
    Sys_Printf("BOOT: creating OpenGX context\n");
	sdl_gl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_gl_context) Sys_Error("SDL_GL_CreateContext: %s", SDL_GetError());
#ifdef PLATFORM_WII
    Wii_BootGraphicsReady();
    VID_WiiPresentation();
#endif
	sdl_fullscreen = !!vid_fullscreen.value;
	if (SDL_GL_SetSwapInterval(r_vsync.value ? 1 : 0) != 0)
		Con_Printf("Could not set VSync: %s\n", SDL_GetError());
	sdl_vsync = !!r_vsync.value;
	SDL_GL_GetDrawableSize(sdl_window, &sdl_window_width, &sdl_window_height);

	Cvar_RegisterVariable(&gl_ztrick);
	vid.maxwarpwidth = vid.width = sdl_window_width;
	vid.maxwarpheight = vid.height = sdl_window_height;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.conwidth = 640;
	vid.conheight = 480;
	vid.aspect = ((float)vid.height / vid.width) * (320.0f / 240.0f);
	vid.scale = vid.height / STD_UI_HEIGHT;
	if (vid.scale < 1) vid.scale = 1;
	vid.numpages = 2;
	/* Use the same logical dimensions at startup and after a mode change. */
	vid.width = 0;
	VID_SDLResize();
	GL_Init();
	Check_Gamma(palette);
	VID_SetPalette(palette);
	Con_SafePrintf("SDL OpenGL video mode %dx%d initialized.\n", sdl_window_width, sdl_window_height);
}

void VID_Shutdown(void)
{
#ifdef PLATFORM_WII
    /* Consume any queued UI draws before destroying the OpenGX context. */
    if (sdl_gl_context) {
        Draw_Flush();
        glFinish();
    }
#endif
	if (sdl_gl_context) SDL_GL_DeleteContext(sdl_gl_context);
	if (sdl_window) SDL_DestroyWindow(sdl_window);
	sdl_gl_context = NULL;
	sdl_window = NULL;
}
