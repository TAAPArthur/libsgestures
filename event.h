#ifndef LIB_SGESUTRES_EVENT_H_
#define LIB_SGESUTRES_EVENT_H_

#include "gestures.h"

/// @return the gesture region id or 0
#define GESTURE_REGION_ID(G) (G->id >> 32)
/**
 * @return The libinput device id
 */
#define GESTURE_DEVICE_ID(G) ((ProductID)G->id)
/**
 * Gesture specific UserEvent
 */
typedef struct {
    /// identifier use to potentially combine identical gestures
    uint32_t seq;
    /// identifier use to potentially combine identical gestures
    GestureGroupID id;
    /// Describes the gesture
    GestureDetail detail;
    /// Used to match GestureBindings
    GestureFlags flags;
    /// the time this event was created
    uint32_t time;
    /// The last point of the gesture
    GesturePoint endPoint;
    GesturePoint endPixelPoint;
} GestureEvent ;
/**
 * Gesture specific bindings
 */
typedef struct {
    /// special flags specific to gestures
    const GestureDetail detail;
    void(*const func)();
    const GestureFlags minFlags;
    const GestureFlags maxFlags;
    ProductID regionID;
    ProductID deviceID;
} GestureBinding ;

bool matchesGestureEvent(GestureBinding*binding, const GestureEvent* event);
bool matchesGestureFlags(GestureBinding*binding, const GestureFlags* flags);

/**
 * @return the number of queued events
 */
int getGestureEventQueueSize();

GestureEvent* waitForNextGesture();
GestureEvent* getNextGesture();
bool isNextGestureReady();
uint32_t getGestureQueueSize();

void dumpGesture(GestureEvent* event);
#endif
