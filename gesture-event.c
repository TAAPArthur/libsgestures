#define _POSIX_C_SOURCE  199309L

#include <assert.h>
#include <errno.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gestures.h"
#include "event.h"

#define GESTURE_MERGE_DELAY_TIME 200

#define MAX_BUFFER_SIZE (1<<10)
/**
 * This class is intended to have a 1 read thread and one thread write.
 * Multiple threads trying to read or trying to write will cause problems
 *
 */
typedef struct RingBuffer {
    GestureEvent* eventBuffer[MAX_BUFFER_SIZE];
    /// next index to read from
    uint16_t bufferIndexRead;
    /// next index to write to
    uint16_t bufferIndexWrite;
} RingBuffer;
static inline uint32_t getBufferSize(RingBuffer* buffer) {
    if(buffer->bufferIndexWrite >= buffer->bufferIndexRead)
        return buffer->bufferIndexWrite - buffer->bufferIndexRead;
    return buffer->bufferIndexWrite + (MAX_BUFFER_SIZE - buffer->bufferIndexRead);
}
static inline bool isBufferFull(RingBuffer* buffer) {return getBufferSize(buffer) == MAX_BUFFER_SIZE ;}
static inline GestureEvent* bufferPeek(RingBuffer* buffer) {return buffer->eventBuffer[buffer->bufferIndexRead % MAX_BUFFER_SIZE];}
static inline bool bufferPush(RingBuffer* buffer, GestureEvent* event) {
    if(!isBufferFull(buffer) && event) {
        buffer->eventBuffer[buffer->bufferIndexWrite++ % MAX_BUFFER_SIZE] = event;
    }
    return event && !isBufferFull(buffer);
}
static inline GestureEvent* bufferPop(RingBuffer* buffer) {
    return buffer->eventBuffer[buffer->bufferIndexRead++ % MAX_BUFFER_SIZE];
}



static uint32_t gestureSelectMask = -1;
void listenForGestureEvents(uint32_t mask) {
    gestureSelectMask = mask;
}
static void (*gestureEventHandler)(GestureEvent* event) = dumpAndFreeGesture;
void registerEventHandler(void (*handler)(GestureEvent* event)) {
    gestureEventHandler = handler ? handler : dumpAndFreeGesture;
}

void enqueueEvent(GestureEvent* event) {
    assert(event);
    if (event->flags.mask & gestureSelectMask) {
        GestureEvent* reflectionEvent = NULL;
        if (event->flags.reflectionMask) {
            reflectionEvent = malloc(sizeof(GestureEvent));
            memcpy(reflectionEvent, event, sizeof(GestureEvent));
            if(reflectionEvent->flags.reflectionMask == Rotate90Mask)
                reflectionEvent->flags.reflectionMask = Rotate270Mask;
            else if(reflectionEvent->flags.reflectionMask == Rotate270Mask)
                reflectionEvent->flags.reflectionMask = Rotate90Mask;
            transformGestureDetail(reflectionEvent->detail, reflectionEvent->flags.reflectionMask);
        }
        gestureEventHandler(event);
        if (reflectionEvent) {
            gestureEventHandler(reflectionEvent);
        }
    }
    else {
        free(event);
    }
}

#define INRANGE(field) contains(binding->minFlags.field,binding->maxFlags.field, flags->field)
static inline bool contains(uint32_t min, uint32_t max, uint32_t value) {
    return (max == 0) ? (min == 0 || min == value) : ((min == 0 || min <= value) && (value <= max));
}

/**
 * @param flags from an event
 * @return 1 iff this GestureBinding's flags are compatible with flags
 */
bool matchesGestureFlags(GestureBindingArg* binding, const GestureFlags* flags) {
    return INRANGE(avgSqDistance) &&
        INRANGE(duration) &&
        INRANGE(fingers) &&
        INRANGE(totalSqDistance) &&
        ((binding->minFlags.mask ? binding->minFlags.mask : GestureEndMask) & flags->mask) == flags->mask &&
        binding->minFlags.reflectionMask == flags->reflectionMask;
}

bool matchesGestureEvent(GestureBindingArg* binding, const GestureEvent* event) {
    if ((!binding->regionID || binding->regionID == GESTURE_REGION_ID(event)) &&
        (!binding->deviceID || binding->deviceID == GESTURE_DEVICE_ID(event)) &&
        matchesGestureFlags(binding, &event->flags))
        if ((!getNumOfTypes(binding->detail) || areDetailsEqual(binding->detail, event->detail)))
            return 1;
    return 0;
}

void dumpGesture(GestureEvent* event) {
    printf("%s: Fingers %d duration %dms", getGestureMaskString(event->flags.mask),
            event->flags.fingers, event->flags.duration);
    for (int i = 0; i < getNumOfTypes(event->detail); i++) {
        printf(" %s", getGestureTypeString(getGestureType(event->detail, i)));
    }
    printf("\n");
}

void dumpAndFreeGesture(GestureEvent* event) {
    dumpGesture(event);
    free(event);
}
