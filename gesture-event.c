#define _POSIX_C_SOURCE  199309L

#include "assert.h"
#include "stdio.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <linux/input.h>
#include <pthread.h>

#include "gestures.h"
#include "event.h"

#define GESTURE_MERGE_DELAY_TIME 200

#define MAX_BUFFER_SIZE (1<<10)
/**
 * This class is intended to have a 1 read thread and one thread write.
 * Multiple threads trying to read or trying to write will cause problems
 *
 * Meant to be a drop in replacement for std::deque in terms of method signatures
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
static inline bool bufferPush(RingBuffer* buffer, GestureEvent*event) {
    if(!isBufferFull(buffer) && event) {
        buffer->eventBuffer[buffer->bufferIndexWrite++ % MAX_BUFFER_SIZE] = event;
    }
    return event && !isBufferFull(buffer);
}
static inline GestureEvent* bufferPop(RingBuffer* buffer) {
    return buffer->eventBuffer[buffer->bufferIndexRead++ % MAX_BUFFER_SIZE];
}
static RingBuffer gestureQueue;
static RingBuffer touchQueue;
static GestureEvent reflectionEvent;
static bool hasRelectionEvent;

bool ready;
bool isGestures();

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condVar = PTHREAD_COND_INITIALIZER;
void signal() {
    pthread_mutex_lock(&mutex);
    ready = 1;
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condVar);
}
void justWait() {
    pthread_mutex_lock(&mutex);
    while(!ready) {
        pthread_cond_wait(&condVar, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    ready = 0;
}

static inline unsigned int getTime() {
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000 + start.tv_usec * 1e-3;
}
/// Sleeps until GESTURE_MERGE_DELAY_TIME ms have passed since creation
static inline void waitUntilX(uint32_t time) {
    uint32_t delta = getTime() - time;
    struct timespec ts;
    ts.tv_sec = delta / 1000;
    ts.tv_nsec = (delta % 1000) * 1000000;
    if(delta < GESTURE_MERGE_DELAY_TIME)
        while(nanosleep(&ts, &ts));
}


static uint32_t gestureSelectMask = -1;
void listenForGestureEvents(uint32_t mask) {
    gestureSelectMask = mask;
}
int getGestureEventQueueSize() {
    return getBufferSize(&gestureQueue);
}

GestureEvent* enqueueEvent(GestureEvent* event) {
    assert(event);
    if(event->flags.mask & gestureSelectMask) {
        if(event->flags.mask & GestureEndMask)
            bufferPush(&gestureQueue, event);
        else bufferPush(&touchQueue, event);
    }
    else {
        free(event);
        return NULL;
    }
    signal();
    return event;
}

#define INRANGE(field) contains(binding->minFlags.field,binding->maxFlags.field, flags->field)
static inline bool contains(uint32_t min, uint32_t max, uint32_t value) {
    return (max == 0)? (min == 0 || min == value): ((min == 0 || min <= value) && (value <= max));
}

/**
 * @param flags from an event
 * @return 1 iff this GestureBinding's flags are compatible with flags
 */
bool matchesGestureFlags(GestureBinding*binding, const GestureFlags* flags) {
    return INRANGE(avgSqDistance) &&
        INRANGE(count) &&
        INRANGE(duration) &&
        INRANGE(fingers) &&
        INRANGE(totalSqDistance) &&
        (binding->minFlags.mask == 0 || (binding->minFlags.mask & flags->mask) == flags->mask) &&
        binding->minFlags.reflectionMask == flags->reflectionMask;
}

bool matchesGestureEvent(GestureBinding*binding, const GestureEvent* event) {
    if( (!binding->regionID || binding->regionID == GESTURE_REGION_ID(event)) &&
        (!binding->deviceID || binding->deviceID == GESTURE_DEVICE_ID(event)) &&
        matchesGestureFlags(binding, &event->flags))
        if((!getNumOfTypes(binding->detail) || areDetailsEqual(binding->detail, event->detail)))
            return 1;
    return 0;

}

bool isNextGestureReady() {
    return getGestureQueueSize();
}
uint32_t getGestureQueueSize() {
    return (hasRelectionEvent?1:0) + getBufferSize(&gestureQueue) + getBufferSize(&touchQueue);
}

bool isDuplicate(GestureEvent*event1, GestureEvent*event2) {
        return event1->id == event2->id && areDetailsEqual(event1->detail, event2->detail) && event1->flags.fingers == event2->flags.fingers &&
            event1->flags.mask == event2->flags.mask;
}

GestureEvent* getNextGesture() {

    if(!isNextGestureReady())
        return NULL;
    if(hasRelectionEvent) {
        hasRelectionEvent = 0;
        return &reflectionEvent;
    }
    GestureEvent* event;
    if(!getBufferSize(&touchQueue) || getBufferSize(&gestureQueue) && bufferPeek(&gestureQueue)->seq < bufferPeek(&touchQueue)->seq) {
        event = bufferPop(&gestureQueue);
        waitUntilX(event->time);
        while(getBufferSize(&gestureQueue)) {
            uint32_t delta = bufferPeek(&gestureQueue)->time - event->time;
            if(isDuplicate(event, bufferPeek(&gestureQueue)) && delta <  GESTURE_MERGE_DELAY_TIME) {
                GestureEvent* temp = bufferPop(&gestureQueue);
                waitUntilX(temp->time);
                event->flags.count += 1;
                free(temp);
                continue;
            }
            break;
        }
    }
    else {
        event = bufferPop(&touchQueue);
    }
    if(event->flags.reflectionMask) {
        memcpy(&reflectionEvent, event, sizeof(GestureEvent));
        hasRelectionEvent = 1;
        if(reflectionEvent.flags.reflectionMask == Rotate90Mask)
            reflectionEvent.flags.reflectionMask = Rotate270Mask;
        else if(reflectionEvent.flags.reflectionMask == Rotate270Mask)
            reflectionEvent.flags.reflectionMask = Rotate90Mask;
        reflectionEvent.detail = transformGestureDetail(event->detail, reflectionEvent.flags.reflectionMask);
    }
    assert(event);
    return event;
}
GestureEvent* waitForNextGesture() {
    while(!isNextGestureReady()){
        justWait();
    }
    return getNextGesture();
}
