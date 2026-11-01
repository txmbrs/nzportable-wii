#include "../../nzportable_def.h"
#include "../../menu/menu_defs.h"
#include "sdl_local.h"

#include <errno.h>
#include <limits.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HANDLES 32
#ifdef PLATFORM_WII
#include <fat.h>
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <network.h>
#include "../wii/boot_wii.h"
#include "../wii/render_memory_wii.h"
#include "../wii/chainloader_wii.h"
static char cooperation_path[1024];
static char cooperation_status[160] = "Join NZP / PC servers (experimental Wii client).";
const char *Wii_CooperationStatus(void) { return cooperation_status; }
void Wii_StartCooperation(void)
{
    char base[800],path[1024];
    const char *required[]={"online/compat.pk3","online/progs.pk3","online/cacert.pem","online/tls-seed.bin"};
    unsigned int i;
    struct stat st;
    if(!getcwd(base,sizeof(base))) return;
    snprintf(cooperation_path,sizeof(cooperation_path),"%s/online/client.dol",base);
    if(!Wii_CheckDOL(cooperation_path,cooperation_status,sizeof(cooperation_status))) { cooperation_path[0]=0; return; }
    for(i=0;i<sizeof(required)/sizeof(required[0]);i++) {
        snprintf(path,sizeof(path),"%s/%s",base,required[i]);
        if(stat(path,&st) || st.st_size<=0 || (i==3 && st.st_size!=64)) {
            snprintf(cooperation_status,sizeof(cooperation_status),"Missing %s. Run Install-Cooperation.ps1.",required[i]);
            cooperation_path[0]=0;
            return;
        }
    }
    Sys_Quit();
}
#define DEFAULT_MEMORY_MB 40
void Wii_PollInput(void);
#else
#define DEFAULT_MEMORY_MB 128
#endif

static FILE *sys_handles[MAX_HANDLES];
qboolean isDedicated;
qboolean sdl_running = true;

static qboolean SDL_ArgumentsRequestHeadless(int argc, char **argv)
{
	int i;

	for (i = 1; i + 1 < argc; ++i)
		if (!strcmp(argv[i], "+vid_renderer") &&
			!Q_strcasecmp(argv[i + 1], "headless"))
			return true;
	return false;
}

static int Sys_FindHandle(void)
{
	int i;
	for (i = 1; i < MAX_HANDLES; ++i)
		if (!sys_handles[i]) return i;
	Sys_Error("out of handles");
	return -1;
}

static int Sys_FileLength(FILE *file)
{
	long position = ftell(file);
	long length;
    if(position<0||fseek(file,0,SEEK_END))return -1;
	length = ftell(file);
    if(length<0||length>INT_MAX||fseek(file,position,SEEK_SET))return -1;
	return (int)length;
}

int Sys_FileOpenRead(char *path, int *handle)
{
	FILE *file = fopen(path, "rb");
	int index;
	if (!file) { *handle = -1; return -1; }
	index = Sys_FindHandle();
	sys_handles[index] = file;
	*handle = index;
    int length=Sys_FileLength(file);
    if(length<0){fclose(file);sys_handles[index]=NULL;*handle=-1;}
    return length;
}

int Sys_FileOpenWrite(char *path)
{
	FILE *file = fopen(path, "wb");
	int index;
	if (!file) Sys_Error("Error opening %s: %s", path, strerror(errno));
	index = Sys_FindHandle();
	sys_handles[index] = file;
	return index;
}

static FILE *Sys_CheckedHandle(int handle){return handle>0&&handle<MAX_HANDLES?sys_handles[handle]:NULL;}
void Sys_FileClose(int handle) { FILE *file=Sys_CheckedHandle(handle);if(file){fclose(file);sys_handles[handle]=NULL;} }
void Sys_FileSeek(int handle, int position) { FILE *file=Sys_CheckedHandle(handle);if(!file||fseek(file,position,SEEK_SET))Sys_Error("Invalid file seek: handle=%d offset=%d",handle,position); }
int Sys_FileRead(int handle, void *dest, int count) { FILE *file=Sys_CheckedHandle(handle);return file&&count>=0?(int)fread(dest,1,count,file):-1; }
int Sys_FileWrite(int handle, void *data, int count) { FILE *file=Sys_CheckedHandle(handle);return file&&count>=0?(int)fwrite(data,1,count,file):-1; }
int Sys_FileTime(char *path) {

	#if defined(_WIN32)
	struct _stat st;
	return _stat(path, &st) == 0 ? (int)st.st_mtime : -1;
	#else
	struct stat st;
	return stat(path, &st) == 0 ? (int)st.st_mtime : -1;
	#endif
}
void Sys_mkdir(char *path)
{ 
	#if defined(_WIN32)
	_mkdir(path);
	#else
	mkdir(path, 0777);
	#endif
}
void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length) { (void)startaddr; (void)length; }

void Sys_PrintSystemInfo(void) { Con_Printf("Vril Engine SDL (%s)\n", SDL_GetPlatform()); }
void Sys_Printf(char *fmt, ...) { va_list args; va_start(args, fmt);
#ifdef PLATFORM_WII
    char message[2048];vsnprintf(message,sizeof(message),fmt,args);Wii_BootMessage(message);
#else
    vfprintf(stdout, fmt, args);
#endif
    va_end(args); }
void Sys_SystemError(char *error) {
#ifdef PLATFORM_WII
    Wii_BootFatal(error);
#else
    fprintf(stderr, "Vril Engine: %s\n", error); if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vril Engine", error, sdl_window); SDL_Quit(); exit(1);
#endif
}
void Sys_Quit(void) {
#ifdef PLATFORM_WII
    if (sdl_running) Sys_Printf("QUIT: requested; blocking further rendering\n");
    scr_disabled_for_loading = true;
#endif
    sdl_running = false;
}
double Sys_FloatTime(void) { static Uint64 start; Uint64 now = SDL_GetPerformanceCounter(); if (!start) start = now; return (double)(now - start) / SDL_GetPerformanceFrequency(); }
char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) { SDL_Delay(1); }
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}
void Sys_SetFPCW(void) {}
void Sys_DebugLog(char *file, char *fmt, ...) { (void)file; (void)fmt; }
void Sys_CaptureScreenshot(void)
{
	SDL_Surface *screenshot;
	unsigned char *pixels;
	int width = vid.width;
	int height = vid.height;
	int row_size = width * 3;
	int y;

	pixels = malloc((size_t)row_size * height);
	if (!pixels) {
		Con_Printf("Could not allocate screenshot buffer.\n");
		return;
	}

	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	screenshot = SDL_CreateRGBSurface(0, width, height, 24,
		0xff0000, 0x00ff00, 0x0000ff, 0);
#else
	screenshot = SDL_CreateRGBSurface(0, width, height, 24,
		0x0000ff, 0x00ff00, 0xff0000, 0);
#endif
	if (!screenshot) {
		Con_Printf("Could not create screenshot surface: %s\n", SDL_GetError());
		free(pixels);
		return;
	}

	for (y = 0; y < height; ++y)
		memcpy((byte *)screenshot->pixels + y * screenshot->pitch,
			pixels + (height - y - 1) * row_size, row_size);

	if (SDL_SaveBMP(screenshot, "capture.bmp") != 0)
		Con_Printf("Could not save capture.bmp: %s\n", SDL_GetError());

	SDL_FreeSurface(screenshot);
	free(pixels);
}

void Sys_DefaultConfig(void)
{
	Cbuf_AddText("bind MOUSE1 +attack\n");
	Cbuf_AddText("bind MOUSE2 +aim\n");
	Cbuf_AddText("bind w +forward\n");
	Cbuf_AddText("bind s +back\n");
	Cbuf_AddText("bind a +moveleft\n");
	Cbuf_AddText("bind d +moveright\n");
	Cbuf_AddText("bind SPACE +jump\n");
	Cbuf_AddText("bind MWHEELUP +switch\n");
	Cbuf_AddText("bind MWHEELDOWN +switch\n");
	Cbuf_AddText("bind ` toggleconsole\n");
}


int mouse_dx;
int mouse_dy;

static int SDL_KeyToQuake(SDL_Keycode key)
{
	switch (key) {
	case SDLK_UP: return K_UPARROW; case SDLK_DOWN: return K_DOWNARROW;
	case SDLK_LEFT: return K_LEFTARROW; case SDLK_RIGHT: return K_RIGHTARROW;
	case SDLK_ESCAPE: return K_ESCAPE; case SDLK_RETURN: case SDLK_KP_ENTER: return K_ENTER;
	case SDLK_TAB: return K_TAB; case SDLK_BACKSPACE: return '\b';
	case SDLK_DELETE: return K_DELETE;
	case SDLK_F1: return K_AUX1; case SDLK_F2: return K_AUX2; case SDLK_F3: return K_AUX3;
	case SDLK_F4: return K_AUX4; case SDLK_F5: return K_AUX5; case SDLK_F6: return K_AUX6;
	case SDLK_F7: return K_AUX7; case SDLK_F8: return K_AUX8; case SDLK_F9: return K_AUX9;
	case SDLK_F10: return K_AUX10; case SDLK_F11: return K_AUX11; case SDLK_F12: return K_AUX12;
	case SDLK_F13: return K_AUX13; case SDLK_F14: return K_AUX14; case SDLK_F15: return K_AUX15;
	case SDLK_F16: return K_AUX16; case SDLK_F17: return K_AUX17; case SDLK_F18: return K_AUX18;
	case SDLK_F19: return K_AUX19; case SDLK_F20: return K_AUX20; case SDLK_F21: return K_AUX21;
	case SDLK_F22: return K_AUX22; case SDLK_F23: return K_AUX23; case SDLK_F24: return K_AUX24;
	case SDLK_LSHIFT: case SDLK_RSHIFT: return K_SHIFT;
	case SDLK_LCTRL: case SDLK_RCTRL: return K_CTRL;
	case SDLK_LALT: case SDLK_RALT: return K_ALT;
	case SDLK_HOME: return K_HOME; case SDLK_END: return K_END;
	case SDLK_PAGEUP: return K_PGUP; case SDLK_PAGEDOWN: return K_PGDN;
	case SDLK_INSERT: return K_INSERT; case SDLK_PAUSE: return K_PAUSE;
	case SDLK_CAPSLOCK: return K_CAPSLOCK; case SDLK_NUMLOCKCLEAR: return K_NUMLOCK;
	case SDLK_SCROLLLOCK: return K_SCROLLLOCK; case SDLK_PRINTSCREEN: return K_PRINTSCREEN;
	case SDLK_KP_9: return K_KP_9; case SDLK_KP_PERIOD: return K_KP_PERIOD;
	case SDLK_KP_DIVIDE: return K_KP_DIVIDE; case SDLK_KP_MULTIPLY: return K_KP_MULTIPLY;
	case SDLK_KP_MINUS: return K_KP_MINUS; case SDLK_KP_PLUS: return K_KP_PLUS;
	case SDLK_KP_EQUALS: return K_KP_EQUALS;
	case SDLK_APPLICATION: return K_APPLICATION; case SDLK_POWER: return K_POWER;
	case SDLK_HELP: return K_HELP; case SDLK_MENU: return K_MENU; case SDLK_SELECT: return K_SELECT_KEY;
	case SDLK_STOP: return K_STOP; case SDLK_AGAIN: return K_AGAIN; case SDLK_UNDO: return K_UNDO;
	case SDLK_CUT: return K_CUT; case SDLK_COPY: return K_COPY; case SDLK_PASTE: return K_PASTE;
	case SDLK_FIND: return K_FIND; case SDLK_MUTE: return K_MUTE; case SDLK_VOLUMEUP: return K_VOLUMEUP;
	case SDLK_VOLUMEDOWN: return K_VOLUMEDOWN; case SDLK_SYSREQ: return K_SYSREQ; case SDLK_CLEAR: return K_CLEAR;
	case SDLK_SPACE: return K_SPACE;
	default:
		if (key >= 32 && key < 127) return (int)key;
		{
			SDL_Scancode scancode = SDL_GetScancodeFromKey(key);
			return scancode != SDL_SCANCODE_UNKNOWN ? K_SDL_SCANCODE_BASE + scancode : 0;
		}
	}
}

static int SDL_StringToKeynum(const char *name)
{
	SDL_Scancode scancode = SDL_GetScancodeFromName(name);
	return scancode != SDL_SCANCODE_UNKNOWN ? K_SDL_SCANCODE_BASE + scancode : -1;
}

static const char *SDL_KeynumToString(int keynum)
{
	if (keynum < K_SDL_SCANCODE_BASE || keynum >= MAX_KEYS)
		return NULL;
	return SDL_GetScancodeName((SDL_Scancode)(keynum - K_SDL_SCANCODE_BASE));
}

static int SDL_MouseToQuake(Uint8 button)
{
	switch (button) { case SDL_BUTTON_LEFT: return K_MOUSE1; case SDL_BUTTON_RIGHT: return K_MOUSE2; case SDL_BUTTON_MIDDLE: return K_MOUSE3; case SDL_BUTTON_X1: return K_MOUSE4; case SDL_BUTTON_X2: return K_MOUSE5; default: return 0; }
}

static int SDL_ControllerToQuake(SDL_GameControllerButton button)
{
	switch (button) {
	case SDL_CONTROLLER_BUTTON_A: return K_BOTTOMFACE;
	case SDL_CONTROLLER_BUTTON_B: return K_RIGHTFACE;
	case SDL_CONTROLLER_BUTTON_X: return K_LEFTFACE;
	case SDL_CONTROLLER_BUTTON_Y: return K_TOPFACE;
	case SDL_CONTROLLER_BUTTON_BACK: return K_SELECT;
	case SDL_CONTROLLER_BUTTON_START: return K_START;
	case SDL_CONTROLLER_BUTTON_LEFTSTICK: return K_LTHUMB;
	case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return K_RTHUMB;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return K_LTRIGGER;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_RTRIGGER;
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return K_DPAD_UP;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return K_DPAD_DOWN;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return K_DPAD_LEFT;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_DPAD_RIGHT;
	default: return 0;
	}
}

static void SDL_MenuCoordinates(int window_x, int window_y, int *drawable_x, int *drawable_y)
{
	int window_width, window_height;
	SDL_GetWindowSize(sdl_window, &window_width, &window_height);
	*drawable_x = window_width > 0 ? window_x * (int)vid.width / window_width : window_x;
	*drawable_y = window_height > 0 ? window_y * (int)vid.height / window_height : window_y;
}

void Sys_SendKeyEvents(void)
{
#ifdef PLATFORM_WII
    Wii_PollInput();
    if (!sdl_running) return;
#endif
	SDL_Event event;
	static qboolean trigger_down[2];
	if (!SDL_WasInit(SDL_INIT_EVENTS))
		return;
	while (SDL_PollEvent(&event)) {
		int key;
#ifdef PLATFORM_WII
        extern qboolean Wii_NativeButtonActivity(void);
        /* Native WPAD/PAD owns controllers. Keep USB keyboard/mouse events,
         * but do not let the same native button also bind a synthetic mouse. */
        if (event.type>=SDL_CONTROLLERAXISMOTION && event.type<=SDL_CONTROLLERDEVICEREMAPPED) continue;
        if ((event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP) && Wii_NativeButtonActivity()) continue;
#endif
		switch (event.type) {
		case SDL_QUIT: Sys_Quit(); return;
		case SDL_KEYDOWN: case SDL_KEYUP:
			if (event.type == SDL_KEYDOWN) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
			if (event.type == SDL_KEYDOWN && !event.key.repeat &&
				event.key.keysym.sym == SDLK_BACKQUOTE && key_dest == key_menu) {
				Con_ToggleConsole_f();
				break;
			}
			key = SDL_KeyToQuake(event.key.keysym.sym);
			if (key && !event.key.repeat) Key_Event(key, event.type == SDL_KEYDOWN);
			break;
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
			if (event.type == SDL_MOUSEBUTTONDOWN) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
			key = SDL_MouseToQuake(event.button.button);
			if (key && !in_bind && (key_dest == key_menu || key_dest == key_menu_pause)) {
				int x, y;
				qboolean slider_handled = false;
				SDL_MenuCoordinates(event.button.x, event.button.y, &x, &y);
				Menu_MouseMove(x, y);
				if (key == K_MOUSE1)
					slider_handled = Menu_MouseButton(x, y, event.type == SDL_MOUSEBUTTONDOWN);
				if (event.type == SDL_MOUSEBUTTONDOWN && key == K_MOUSE1 && !slider_handled) Menu_ButtonPress();
			} else if (key) Key_Event(key, event.type == SDL_MOUSEBUTTONDOWN);
			break;
		case SDL_MOUSEMOTION:
#ifndef PLATFORM_WII
			if (event.motion.xrel || event.motion.yrel) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
#endif
			if (key_dest == key_menu || key_dest == key_menu_pause) {
				int x, y;
				SDL_MenuCoordinates(event.motion.x, event.motion.y, &x, &y);
				Menu_MouseMove(x, y);
			}
			else { mouse_dx += event.motion.xrel; mouse_dy += event.motion.yrel; }
			break;
		case SDL_MOUSEWHEEL:
			if (in_bind || (key_dest != key_menu && key_dest != key_menu_pause)) {
				key = event.wheel.y > 0 ? K_MWHEELUP : event.wheel.y < 0 ? K_MWHEELDOWN : 0;
				if (key) { Key_Event(key, true); Key_Event(key, false); }
			} else {
				if (event.wheel.y > 0) Menu_IncreaseCursor();
				if (event.wheel.y < 0) Menu_DecreaseCursor();
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
			IN_SDLControllerActivated(event.cbutton.which);
			if (event.type == SDL_CONTROLLERBUTTONDOWN) { IN_SetActiveDevice(IN_DEVICE_GAMEPAD); Menu_SetInputDevice(IN_DEVICE_GAMEPAD); }
			key = SDL_ControllerToQuake(event.cbutton.button);
			if (key) Key_Event(key, event.type == SDL_CONTROLLERBUTTONDOWN);
			break;
		case SDL_CONTROLLERAXISMOTION:
			IN_SDLControllerActivated(event.caxis.which);
			if (event.caxis.value > 8192 || event.caxis.value < -8192) { IN_SetActiveDevice(IN_DEVICE_GAMEPAD); Menu_SetInputDevice(IN_DEVICE_GAMEPAD); }
			if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
				event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
				int trigger = event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : 1;
				qboolean down = event.caxis.value > 16384;
				if (down != trigger_down[trigger]) {
					trigger_down[trigger] = down;
					Key_Event(trigger ? K_ZRTRIGGER : K_ZLTRIGGER, down);
				}
			}
			break;
		case SDL_CONTROLLERDEVICEADDED: IN_SDLControllerAdded(event.cdevice.which); break;
		case SDL_CONTROLLERDEVICEREMOVED: IN_SDLControllerRemoved(event.cdevice.which); break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED)
				VID_SDLResize();
			break;
		}
	}
	SDL_SetRelativeMouseMode((key_dest == key_game && SDL_GetKeyboardFocus() == sdl_window) ? SDL_TRUE : SDL_FALSE);
}

int main(int argc, char **argv)
{
	quakeparms_t parms;
	startup_arguments_t startup;
	const char *base_directory;
	char startup_error[256];
	size_t heap_size;
	double oldtime;
	qboolean headless_test;
#ifdef PLATFORM_WII
    Wii_BootInit(argc,argv);
    Wii_BootMessage("BOOT: loading startup arguments\n");
#endif
	memset(&parms, 0, sizeof(parms));
	if (!Startup_LoadArguments(&startup, argc, argv, "setup.ini",
		startup_error, sizeof(startup_error))) {
		Sys_Printf("Startup: %s\n", startup_error);
#ifdef PLATFORM_WII
        Wii_BootFatal(startup_error);
#endif
		return 1;
	}
	if (!Startup_GetBaseDirectory(&startup, ".", &base_directory,
		startup_error, sizeof(startup_error))) {
		Sys_Printf("Startup: %s\n", startup_error);
#ifdef PLATFORM_WII
        Wii_BootFatal(startup_error);
#endif
		Startup_FreeArguments(&startup);
		return 1;
	}
	headless_test = TestHandler_ArgumentsAllowHeadless(startup.argc, startup.argv) ||
		SDL_ArgumentsRequestHeadless(startup.argc, startup.argv);
    Sys_Printf("BOOT: SDL initialization\n");
#ifdef PLATFORM_WII
	if (SDL_Init(headless_test ? SDL_INIT_TIMER :
		(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) != 0) {
#else
	if (SDL_Init(headless_test ? SDL_INIT_TIMER :
		(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER)) != 0) {
#endif
		Sys_Printf("SDL_Init: %s\n", SDL_GetError());
#ifdef PLATFORM_WII
        Wii_BootFatal(SDL_GetError());
#endif
		Startup_FreeArguments(&startup);
		return 1;
	}
	Key_SetPlatformKeyConversion(SDL_StringToKeynum, SDL_KeynumToString);
    Sys_Printf("BOOT: engine heap allocation\n");
	parms.membase = Startup_AllocateHeap(&startup, DEFAULT_MEMORY_MB * 1024 * 1024,
		&heap_size, startup_error, sizeof(startup_error));
	if (!parms.membase) {
		Sys_Printf("Startup: %s\n", startup_error);
#ifdef PLATFORM_WII
        Wii_BootFatal(startup_error);
#endif
		Startup_FreeArguments(&startup);
		SDL_Quit();
		return 1;
	}
#ifdef PLATFORM_WII
    Wii_RenderMemoryInit();
#endif
	parms.memsize = (int)heap_size;
	parms.basedir = (char *)base_directory;
	COM_InitArgv(startup.argc, startup.argv);
	parms.argc = com_argc;
	parms.argv = com_argv;
    Sys_Printf("BOOT: Host_Init\n");
	Host_Init(&parms);
    Sys_Printf("BOOT: entering game frame loop\n");
	oldtime = Sys_FloatTime();
#ifdef PLATFORM_WII
    Wii_BootSetRuntime(1);
    unsigned int saved_revision = cvar_archive_revision;
    unsigned int pending_revision = saved_revision;
    Uint32 config_save_deadline = 0;
#endif
	while (sdl_running) {
		double now = Sys_FloatTime();
		Host_Frame(now - oldtime);
#ifdef PLATFORM_WII
        if (!sdl_running) break;
#endif
#ifdef PLATFORM_WII
        /* Debounce sliders: one save after the last archived setting change,
         * instead of SD writes on every controller step or only at exit. */
        Uint32 ticks = SDL_GetTicks();
        if (pending_revision != cvar_archive_revision) {
            pending_revision = cvar_archive_revision;
            config_save_deadline = ticks + 1000;
        }
        if (pending_revision != saved_revision &&
            (Sint32)(ticks - config_save_deadline) >= 0) {
            if (Host_WriteConfiguration()) saved_revision = pending_revision;
            else config_save_deadline = ticks + 5000;
        }
#endif
		music_update();
		oldtime = now;
	}
	if (host_initialized)
	{
#ifdef PLATFORM_WII
        Wii_BootSetRuntime(0);
        Sys_Printf("QUIT: disconnecting client/server\n");
        CL_Disconnect_f();
        Sys_Printf("QUIT: saving settings and shutting down subsystems\n");
#endif
		Host_Shutdown();
	}
#ifndef PLATFORM_WII
	free(parms.membase);
#endif
	Startup_FreeArguments(&startup);
#ifdef PLATFORM_WII
    Sys_Printf("QUIT: closing SDL\n");
#endif
	SDL_Quit();
#ifdef PLATFORM_WII
    Sys_Printf("QUIT: complete\n");
    if(cooperation_path[0]) {
        /* Startup_AllocateHeap reserves this hunk directly from the MEM2 arena;
         * it is not a malloc block. With Host/SDL stopped, reuse it for the
         * incoming DOL instead of requesting another large libc allocation. */
        Sys_Printf("COOP: reusing retired MEM2 hunk (%lu bytes) for module\n",(unsigned long)heap_size);
        Wii_ChainloadSetLogger(Wii_BootMessage);
        if(!Wii_ChainloadDOLWithBuffer(cooperation_path,parms.membase,(unsigned int)heap_size,
            cooperation_status,sizeof(cooperation_status))) Wii_BootFatal(cooperation_status);
    }
#endif
	return 0;
}
