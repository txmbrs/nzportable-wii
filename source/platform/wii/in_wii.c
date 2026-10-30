#include "../../nzportable_def.h"
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <SDL.h>
#include "../sdl/sdl_local.h"
#include "../../menu/menu_defs.h"
#include "sprint_wii.h"

static in_analog_stick_t sticks[2];
static unsigned int rumble_until;
static qboolean gamepad_connected;
static unsigned int previous_keys;
static wii_sprint_gesture_t sprint_gesture;
static cvar_t wii_flick_sprint={.name="wii_flick_sprint",.string="1",.flags=CVAR_ARCHIVE};
static const struct {unsigned int wpad,pad;int key;} button_map[]={
    {WPAD_BUTTON_A|WPAD_CLASSIC_BUTTON_A,PAD_BUTTON_A,K_BOTTOMFACE},
    {WPAD_BUTTON_2|WPAD_CLASSIC_BUTTON_B,PAD_BUTTON_B,K_RIGHTFACE},
    {WPAD_BUTTON_MINUS|WPAD_CLASSIC_BUTTON_X,PAD_BUTTON_X,K_LEFTFACE},
    {WPAD_BUTTON_1|WPAD_CLASSIC_BUTTON_Y,PAD_BUTTON_Y,K_TOPFACE},
    {WPAD_BUTTON_PLUS|WPAD_CLASSIC_BUTTON_PLUS,PAD_BUTTON_START,K_START},
    {WPAD_NUNCHUK_BUTTON_Z|WPAD_CLASSIC_BUTTON_ZL|WPAD_CLASSIC_BUTTON_FULL_L,PAD_TRIGGER_L,K_ZLTRIGGER},
    {WPAD_BUTTON_B|WPAD_CLASSIC_BUTTON_ZR|WPAD_CLASSIC_BUTTON_FULL_R,PAD_TRIGGER_R,K_ZRTRIGGER},
    {WPAD_NUNCHUK_BUTTON_C|WPAD_CLASSIC_BUTTON_MINUS,PAD_TRIGGER_Z,K_RTRIGGER},
    {WPAD_BUTTON_UP|WPAD_CLASSIC_BUTTON_UP,PAD_BUTTON_UP,K_DPAD_UP},
    {WPAD_BUTTON_DOWN|WPAD_CLASSIC_BUTTON_DOWN,PAD_BUTTON_DOWN,K_DPAD_DOWN},
    {WPAD_BUTTON_LEFT|WPAD_CLASSIC_BUTTON_LEFT,PAD_BUTTON_LEFT,K_DPAD_LEFT},
    {WPAD_BUTTON_RIGHT|WPAD_CLASSIC_BUTTON_RIGHT,PAD_BUTTON_RIGHT,K_DPAD_RIGHT}
};
static float axis(int value, int center, int low, int high)
{
    int delta = value - center;
    float range = delta < 0 ? center - low : high - center;
    float result = range > 0 ? delta / range : 0;
    return result < -1 ? -1 : result > 1 ? 1 : result;
}
void Wii_PollInput(void)
{
    unsigned int move_source=0;
    WPAD_ScanPads(); unsigned int connected = PAD_ScanPads();
    WPADData *w = WPAD_Data(0);
    unsigned int wp = w && w->err == WPAD_ERR_NONE ? w->btns_h : 0;
    if (wp & (WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME)) {
        Sys_Quit();
        return;
    }
    unsigned int pd = PAD_ButtonsHeld(0);
    gamepad_connected=(w && w->err==WPAD_ERR_NONE)||(connected&1u);
    if(w && w->err==WPAD_ERR_NONE && w->exp.type==WPAD_EXP_CLASSIC){
        if(w->exp.classic.l_shoulder>.5f)wp|=WPAD_CLASSIC_BUTTON_FULL_L;
        if(w->exp.classic.r_shoulder>.5f)wp|=WPAD_CLASSIC_BUTTON_FULL_R;
    }
    unsigned int keys=0;
    for(unsigned i=0;i<sizeof(button_map)/sizeof(button_map[0]);i++){
        if((wp&button_map[i].wpad)||(pd&button_map[i].pad))keys|=1u<<i;
        if((keys^previous_keys)&(1u<<i))Key_Event(button_map[i].key,(keys&(1u<<i))!=0);
    }
    previous_keys=keys;
    if(wp||pd){IN_SetActiveDevice(IN_DEVICE_GAMEPAD);Menu_SetInputDevice(IN_DEVICE_GAMEPAD);}
    memset(sticks, 0, sizeof(sticks));
    if (w && w->err == WPAD_ERR_NONE && w->exp.type == WPAD_EXP_NUNCHUK) {
        move_source=1;
        joystick_t *j = &w->exp.nunchuk.js;
        sticks[0].x = axis(j->pos.x, j->center.x, j->min.x, j->max.x);
        sticks[0].y = axis(j->pos.y, j->center.y, j->min.y, j->max.y);
        if (w->ir.valid) {
            /* IR displacement drives the original gamepad camera pipeline. */
            sticks[1].x = (w->ir.x - 320.0f) / 320.0f;
            sticks[1].y = (240.0f - w->ir.y) / 240.0f;
        }
    } else if(w && w->err==WPAD_ERR_NONE && w->exp.type==WPAD_EXP_CLASSIC){
        move_source=2;
        joystick_t *left=&w->exp.classic.ljs,*right=&w->exp.classic.rjs;
        sticks[0].x=axis(left->pos.x,left->center.x,left->min.x,left->max.x);
        sticks[0].y=axis(left->pos.y,left->center.y,left->min.y,left->max.y);
        sticks[1].x=axis(right->pos.x,right->center.x,right->min.x,right->max.x);
        sticks[1].y=axis(right->pos.y,right->center.y,right->min.y,right->max.y);
    } else if (connected & 1u) {
        move_source=3;
        sticks[0].x = PAD_StickX(0) / 100.0f;
        sticks[0].y = PAD_StickY(0) / 100.0f;
        sticks[1].x = PAD_SubStickX(0) / 100.0f;
        sticks[1].y = PAD_SubStickY(0) / 100.0f;
    }
    /* Use the game's real sprint impulse; stamina and stopping remain in QC.
     * Two forward taps with neutral between; a single held push just walks. */
    if (Wii_SprintGesture(&sprint_gesture,move_source,
        wii_flick_sprint.value && key_dest==key_game && !cl.paused &&
        cls.state==ca_connected && cls.signon==SIGNONS && cl.stats[STAT_HEALTH]>0 &&
        cl.stats[STAT_ZOOM]!=1 && cl.stats[STAT_ZOOM]!=2 && !(keys&(1u<<5)),
        sticks[0].x,sticks[0].y,SDL_GetTicks()))
        Cbuf_AddText("impulse 23\n");
    if (rumble_until && SDL_GetTicks() >= rumble_until) {
        WPAD_Rumble(0, 0); PAD_ControlMotor(0, PAD_MOTOR_STOP); rumble_until = 0;
    }
}
qboolean IN_PlatformHasMouse(void) { return false; }
qboolean IN_PlatformHasGamepad(void) { return gamepad_connected; }
void IN_SetMouseToRelative(bool relative) { (void)relative; }
void IN_PlatformClearPendingInput(void) { memset(sticks, 0, sizeof(sticks)); memset(&sprint_gesture,0,sizeof(sprint_gesture)); }
void IN_PlatformMouseMove(usercmd_t *cmd) { (void)cmd; }
void IN_PlatformSetLightbar(byte r, byte g, byte b) { (void)r; (void)g; (void)b; }
qboolean IN_PlatformGetGyro(float *x, float *y) { *x = *y = 0; return false; }
void IN_PlatformRumble(unsigned short low, unsigned short high, unsigned int duration)
{
    int on = (low || high) && duration;
    WPAD_Rumble(0, on); PAD_ControlMotor(0, on ? PAD_MOTOR_RUMBLE : PAD_MOTOR_STOP);
    rumble_until = on ? SDL_GetTicks() + duration : 0;
}
void IN_PlatformInit(void) { Cvar_SetValue("in_anub_mode", 1); Cvar_RegisterVariable(&wii_flick_sprint); }
void IN_PlatformShutdown(void) { WPAD_Rumble(0, 0); PAD_ControlMotor(0, PAD_MOTOR_STOP); }
void IN_PlatformCommands(void) {}
void IN_PlatformMove(usercmd_t *cmd) { (void)cmd; }
void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value) { *value = sticks[stick == IN_STICK_RIGHT]; }
void IN_SDLControllerAdded(int index) { (void)index; }
void IN_SDLControllerRemoved(SDL_JoystickID id) { (void)id; }
void IN_SDLControllerActivated(SDL_JoystickID id) { (void)id; }
