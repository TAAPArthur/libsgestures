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
    /// id of last touch event
    TouchID lastEventId;
    /// Describes the gesture
    GestureDetail detail;
    /// Used to match GestureBindings
    GestureFlags flags;
    /// the time this event was created
    uint32_t time;

    GesturePoint startPoint;
    GesturePoint startPercentPoint;
    /// The last point of the gesture
    GesturePoint endPoint;
    GesturePoint endPercentPoint;
} GestureEvent ;
/**
 * Gesture specific bindings
 */
typedef struct {
    /// special flags specific to gestures
    const GestureDetail detail;
    const GestureFlags minFlags;
    const GestureFlags maxFlags;
    ProductID regionID;
    ProductID deviceID;
} GestureBindingArg ;

bool matchesGestureEvent(GestureBindingArg* binding, const GestureEvent* event);
bool matchesGestureFlags(GestureBindingArg* binding, const GestureFlags* flags);

void dumpGesture(GestureEvent* event);
void dumpAndFreeGesture(GestureEvent* event);


void registerEventHandler(void (*handler)(GestureEvent* event));
#endif
