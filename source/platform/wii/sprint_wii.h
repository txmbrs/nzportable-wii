/* Double forward stick tap; no game-state or platform dependencies. */
#ifndef NZP_SPRINT_WII_H
#define NZP_SPRINT_WII_H
#include <stdint.h>
typedef struct {
    uint32_t departure, first_tap;
    unsigned source;
    int armed, moving, pending;
} wii_sprint_gesture_t;
static inline int Wii_SprintGesture(wii_sprint_gesture_t *s, unsigned source,
    int enabled, float x, float y, uint32_t now)
{
    if (!enabled || !source || source != s->source) {
        s->source=source; s->armed=s->moving=s->pending=0;
        /* A new input source must first report neutral in a later poll. */
        return 0;
    }
    if (s->pending && (uint32_t)(now-s->first_tap)>400u) s->pending=0;
    if (x>=-0.2f && x<=0.2f && y>=-0.2f && y<=0.2f) {
        s->armed=1; s->moving=0;
        return 0;
    }
    if (!s->armed) return 0;
    if (!s->moving) {s->departure=now; s->moving=1;}
    if (y<0.0f || x < -0.6f || x > 0.6f ||
        (uint32_t)(now-s->departure)>180u) {
        s->armed=s->pending=0;
        return 0;
    }
    if (y>=0.8f) {
        s->armed=0;
        if (s->pending) {s->pending=0;return 1;}
        s->pending=1; s->first_tap=now;
    }
    return 0;
}
#endif
