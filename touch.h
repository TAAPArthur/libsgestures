#ifndef LIB_SGESUTRES_TOUCH_H_
#define LIB_SGESUTRES_TOUCH_H_
#include <stdbool.h>
#include <stdint.h>

#define DEVICE_NAME_LEN 64

/// @{ GestureMasks
/// Triggered when a gesture group ends
#define GestureEndMask      (1 << 0)
/// triggered when a seat end
#define TouchEndMask        (1 << 1)
/// triggered when a seat starts
#define TouchStartMask      (1 << 2)
/// triggered when a  new point is added to a gesture
#define TouchHoldMask       (1 << 3)
/// triggered when a new point detected but not added to a gesture
#define TouchMotionMask     (1 << 4)
/// triggered when a touch is cancelled
#define TouchCancelMask     (1 << 5)
/// @}
typedef uint8_t GestureMask ;


/**
 * Helper class for Rect
 */
typedef struct {
    /// x coordinate
    int32_t x;
    /// y coordinate
    int32_t y;
} GesturePoint ;

typedef struct TouchEvent {
    uint32_t id;
    int32_t seat;
    GesturePoint point;
    GesturePoint pointPercent;
    uint32_t time;
} TouchEvent ;


bool readTouchEvent(uint32_t fd);
bool isTouchEventReady(int32_t fd);

typedef struct {
    GestureMask mask;
    TouchEvent touchEvent;
    char totalNameLen;
    char names[];
} RawGestureEvent;

typedef struct {
    RawGestureEvent event;
    char buffer[255];
} LargestRawGestureEvent;


static inline int writeTouchEvent(int fd, const RawGestureEvent* event) {
    return write(fd, event, sizeof(RawGestureEvent) + event->totalNameLen);
}

static inline void setRawGestureEventNames(LargestRawGestureEvent* event, const char* sysname, const char* devname) {
    char* dest = strncpy(event->buffer, sysname, DEVICE_NAME_LEN);
    *dest = 0;
    dest = strncpy(dest + 1, devname, DEVICE_NAME_LEN);
    *dest = 0;
    event->event.totalNameLen = dest - event->buffer;
}

/**
 * Starts a gesture
 *
 * @param id device id
 * @param seat seat number
 * @param startingGesturePoint
 * @param lastGesturePoint
 * @param sysName
 * @param name
 */
void startGesture(const TouchEvent event, const char* sysName, const char* name);
/**
 * Adds a point to a gesture
 *
 * @param id
 * @param seat
 * @param point
 * @param lastGesturePoint
 */
void continueGesture(const TouchEvent event);
/**
 * Concludes a gestures and if all gestures in the GestureGroup are done, termines the group and returns a GestureEvent.
 * The event is already added to the queue for processing
 *
 * @param id
 * @param seat
 *
 * @return NULL of a gesture event if all gestures have completed
 */
void endGesture(const TouchEvent event);
/**
 * Aborts and ongoing gesture
 *
 * Does not generate any events. Already generated events are not affected
 *
 * @param id
 * @param seat
 */
void cancelGesture(const TouchEvent event);
#endif
