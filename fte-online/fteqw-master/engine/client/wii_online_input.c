/* Native controllers for FTE's existing gamepad input path. GPL-2.0-or-later. */
#include "quakedef.h"
#include <wiiuse/wpad.h>
#include <ogc/pad.h>

static unsigned int previous_buttons;
static int pressed_keys[12];
static double wheel_repeat[12];
static qboolean previous_menu;
static float pointer_x=320, pointer_y=240;
static double pointer_time;
void WiiOnline_HomeExit(void);
static const struct { unsigned int wiimote, gamecube; int key; } buttons[] = {
    {WPAD_BUTTON_A|WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, K_GP_DIAMOND_DOWN},
    {WPAD_BUTTON_2|WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B, K_GP_DIAMOND_RIGHT},
    {WPAD_BUTTON_MINUS|WPAD_CLASSIC_BUTTON_X, PAD_BUTTON_X, K_GP_DIAMOND_LEFT},
    {WPAD_BUTTON_1|WPAD_CLASSIC_BUTTON_Y, PAD_BUTTON_Y, K_GP_DIAMOND_UP},
    {WPAD_BUTTON_PLUS|WPAD_CLASSIC_BUTTON_PLUS, PAD_BUTTON_START, K_GP_MENU},
    {WPAD_NUNCHUK_BUTTON_Z|WPAD_CLASSIC_BUTTON_ZL|WPAD_CLASSIC_BUTTON_FULL_L, PAD_TRIGGER_L, K_GP_LEFT_TRIGGER},
    {WPAD_BUTTON_B|WPAD_CLASSIC_BUTTON_ZR|WPAD_CLASSIC_BUTTON_FULL_R, PAD_TRIGGER_R, K_GP_RIGHT_TRIGGER},
    {WPAD_NUNCHUK_BUTTON_C|WPAD_CLASSIC_BUTTON_MINUS, PAD_TRIGGER_Z, K_GP_RIGHT_SHOULDER},
    {WPAD_BUTTON_UP|WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP, K_GP_DPAD_UP},
    {WPAD_BUTTON_DOWN|WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN, K_GP_DPAD_DOWN},
    {WPAD_BUTTON_LEFT|WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT, K_GP_DPAD_LEFT},
    {WPAD_BUTTON_RIGHT|WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT, K_GP_DPAD_RIGHT}
};
static float stick(int value, int center, int low, int high)
{
    float delta = value-center;
    float range = delta < 0 ? center-low : high-center;
    float result = range > 0 ? delta/range : 0;
    return bound(-1, result, 1);
}
void WiiOnline_PollControllers(void)
{
    unsigned int connected, wp, pad, held=0, i;
    float lx=0, ly=0, rx=0, ry=0;
    WPADData *w;
    qboolean menu = Key_Dest_Has(kdm_menu|kdm_prompt);
    WPAD_ScanPads();
    connected = PAD_ScanPads();
    w = WPAD_Data(0);
    wp = w && w->err == WPAD_ERR_NONE ? w->btns_h : 0;
    pad = connected&1 ? PAD_ButtonsHeld(0) : 0;
    if (wp & (WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME)) {
        WiiOnline_HomeExit();
        Cbuf_AddText("quit force\n", RESTRICT_LOCAL);
        return;
    }
    if (w && w->err == WPAD_ERR_NONE && w->exp.type == WPAD_EXP_CLASSIC) {
        joystick_t *left=&w->exp.classic.ljs, *right=&w->exp.classic.rjs;
        lx=stick(left->pos.x,left->center.x,left->min.x,left->max.x);
        ly=stick(left->pos.y,left->center.y,left->min.y,left->max.y);
        rx=stick(right->pos.x,right->center.x,right->min.x,right->max.x);
        ry=stick(right->pos.y,right->center.y,right->min.y,right->max.y);
        if (w->exp.classic.l_shoulder > .5f) wp |= WPAD_CLASSIC_BUTTON_FULL_L;
        if (w->exp.classic.r_shoulder > .5f) wp |= WPAD_CLASSIC_BUTTON_FULL_R;
    } else if (w && w->err == WPAD_ERR_NONE && w->exp.type == WPAD_EXP_NUNCHUK) {
        joystick_t *j=&w->exp.nunchuk.js;
        lx=stick(j->pos.x,j->center.x,j->min.x,j->max.x);
        ly=stick(j->pos.y,j->center.y,j->min.y,j->max.y);
        if (w->ir.valid) { rx=bound(-1,(w->ir.x-320)/320,1); ry=bound(-1,(240-w->ir.y)/240,1); }
    } else if (connected&1) {
        lx=bound(-1,PAD_StickX(0)/100.0f,1); ly=bound(-1,PAD_StickY(0)/100.0f,1);
        rx=bound(-1,PAD_SubStickX(0)/100.0f,1); ry=bound(-1,PAD_SubStickY(0)/100.0f,1);
    }
    for (i=0;i<countof(buttons);++i)
        if ((wp&buttons[i].wiimote) || (pad&buttons[i].gamecube)) held |= 1u<<i;
    if (menu != previous_menu) {
        for (i=0;i<countof(buttons);++i) {
            if (pressed_keys[i]) IN_KeyEvent(0, false, pressed_keys[i], 0);
            pressed_keys[i]=0;
            wheel_repeat[i]=0;
        }
        /* A held menu click must not become a gameplay action (or vice versa). */
        previous_buttons=held;
        previous_menu=menu;
        Sys_Printf("INPUT: %s\n", menu ? "menu pointer; A click, +/- scroll (point at list), B/2 back, HOME exit" : "gamepad controls");
    }
    if (menu) {
        double now=Sys_DoubleTime();
        float dt=pointer_time ? bound(0,now-pointer_time,0.05) : 0;
        float dx=fabs(lx)>.2f?lx:0, dy=fabs(ly)>.2f?-ly:0;
        pointer_time=now;
        if (held&(1u<<8)) dy=-1;
        if (held&(1u<<9)) dy=1;
        if (held&(1u<<10)) dx=-1;
        if (held&(1u<<11)) dx=1;
        if (w && w->err==WPAD_ERR_NONE && w->ir.valid) {
            pointer_x=w->ir.x*vid.pixelwidth/640.0f;
            pointer_y=w->ir.y*vid.pixelheight/480.0f;
        } else {
            pointer_x+=dx*360*dt;
            pointer_y+=dy*360*dt;
        }
        pointer_x=bound(0,pointer_x,vid.pixelwidth-1);
        pointer_y=bound(0,pointer_y,vid.pixelheight-1);
        IN_MouseMove(0,true,pointer_x,pointer_y,0,0);
    } else pointer_time=0;
    for (i=0;i<countof(buttons);++i) {
        unsigned int bit=1u<<i;
        if ((held&bit) && !(previous_buttons&bit)) {
            int key=buttons[i].key;
            if (menu) {
                if (i==0) key=K_MOUSE1;
                else if (i==2) key=K_MWHEELUP;
                else if (i==4) key=K_MWHEELDOWN;
                else if (i==1 || i==6) key=K_ESCAPE;
                else key=0;
            }
            pressed_keys[i]=key;
            if (key) IN_KeyEvent(0,true,key,0);
            if (key==K_MWHEELUP || key==K_MWHEELDOWN)
                wheel_repeat[i]=Sys_DoubleTime()+0.35;
        } else if (menu && (held&bit) && (previous_buttons&bit) &&
                   (pressed_keys[i]==K_MWHEELUP || pressed_keys[i]==K_MWHEELDOWN) &&
                   Sys_DoubleTime()>=wheel_repeat[i]) {
            /* Repeat only deliberate held scroll buttons. Pair releases so
             * menu QC receives distinct wheel presses, with no stuck key. */
            IN_KeyEvent(0,false,pressed_keys[i],0);
            IN_KeyEvent(0,true,pressed_keys[i],0);
            wheel_repeat[i]=Sys_DoubleTime()+0.12;
        } else if (!(held&bit) && (previous_buttons&bit)) {
            if (pressed_keys[i]) IN_KeyEvent(0,false,pressed_keys[i],0);
            pressed_keys[i]=0;
            wheel_repeat[i]=0;
        }
    }
    previous_buttons=held;
    IN_JoystickAxisEvent(0,GPAXIS_LT_RIGHT,menu?0:lx);
    IN_JoystickAxisEvent(0,GPAXIS_LT_DOWN,menu?0:-ly);
    IN_JoystickAxisEvent(0,GPAXIS_RT_RIGHT,menu?0:rx);
    IN_JoystickAxisEvent(0,GPAXIS_RT_DOWN,menu?0:-ry);
}
