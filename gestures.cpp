#include <assert.h>
#include <condition_variable>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <mutex>
#include <stdbool.h>
#include <string.h>
#include <sys/poll.h>
#include <thread>
#include <unordered_map>
#include <vector>
#include <iostream>

#include "time.h"
#include "gestures.h"
#include "gestures-helper.h"
#include "gestures-private.h"

#define GESTURE_MERGE_DELAY_TIME 200

/**
 * This class is intended to have a 1 read thread and one thread write.
 * Multiple threads trying to read or trying to write will cause problems
 *
 * Meant to be a drop in replacement for std::deque in terms of method signatures
 *
 * @tparam T the type to hold
 * @tparam I the size of the buffer
 */
template < class T, int I = 1 << 10 >
class RingBuffer {
    T eventBuffer[I];
    /// next index to read from
    uint16_t bufferIndexRead = 0;
    /// next index to write to
    uint16_t bufferIndexWrite = 0;
    volatile uint16_t _size = 0;
public:
    uint16_t size() const {return _size;}
    uint16_t getMaxSize() const {return I;}
    bool isFull() const {return size() == getMaxSize();}
    bool empty() const {return !size();}
    T pop_front() {
        if(empty())
            return NULL;
        auto element = eventBuffer[bufferIndexRead++ % getMaxSize()];
        _size--;
        return element;
    }
    T front() {
        return eventBuffer[bufferIndexRead % getMaxSize()];
    }
    T back() {
        return eventBuffer[(getMaxSize() + bufferIndexWrite - 1) % getMaxSize()];
    }
    bool push_back(T event) {
        if(!isFull() && event) {
            eventBuffer[bufferIndexWrite++ % getMaxSize()] = event;
            _size++;
        }
        return event && !isFull();
    }
};
static RingBuffer<GestureEvent*> gestureQueue;
static RingBuffer<GestureEvent*> touchQueue;
static GestureEvent*reflectionEvent = NULL;

static std::mutex m;
static std::condition_variable cv;
bool ready;
bool isGestures();
void signal() {
    {
        std::lock_guard<std::mutex> lk(m);
        ready = 1;
    }
    cv.notify_one();
}

void justWait() {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&] {return ready;});
    ready = 0;
}

uint32_t GestureEvent::seqCounter = 0;

static uint32_t gestureSelectMask = -1;
void listenForGestureEvents(uint32_t mask) {
    gestureSelectMask = mask;
}
int getGestureEventQueueSize() {
    return gestureQueue.size();
}

GestureEvent* enqueueEvent(GestureEvent* event) {
    assert(event);
    if(event->flags.mask & gestureSelectMask) {
        if(event->flags.mask & GestureEndMask)
            gestureQueue.push_back(event);
        else touchQueue.push_back(event);
    }
    else {
        delete event;
        return NULL;
    }
    signal();
    return event;
}

/**
 * @param stream
 * @param type
 * @return
 */
static inline std::ostream& operator<<(std::ostream& stream, const GestureType& type) {
    return stream << getGestureTypeString(type);
}

static inline std::ostream& operator>>(std::ostream& stream, const GestureFlags& flags) {
    return stream << "{ " << "mask:" << flags.mask << ", count: " << flags.count << ", sqDis: " << flags.totalSqDistance <<
        ", avgSqDisplacement: " << flags.avgSqDisplacement << ", avgSqDis: " << flags.avgSqDistance <<
        ", duration: " << flags.duration << " fingers: " << flags.fingers << ", Reflection: " << flags.reflectionMask;
}
static inline std::ostream& operator<<(std::ostream& stream, const MinMax& minMax) {
    return minMax.min == minMax.max ? stream << "[" << minMax.max << "]" :
        stream << "[" << minMax.min << ", " << minMax.max << "]";
}
static inline std::ostream& operator<<(std::ostream& stream, const GestureFlags& flags) {
    return stream << "{ " << "count: " << flags.count << ", sqDis: " << flags.totalSqDistance <<
        ", avgSqDisplacement: " << flags.avgSqDisplacement << ", avgSqDis: " << flags.avgSqDistance <<
        ", duration: " << flags.duration << " fingers: " << flags.fingers << ", Reflection: " << flags.reflectionMask;
}

static inline std::ostream& operator<<(std::ostream& stream, const GestureDetail& detail) {
    stream << "{ ";
    for(int i = 0; i < detail.size(); i++)
        stream <<detail[i] << "("<< (int)detail[i]<<"),";
    return stream << "}";
}

/**
 * @param stream
 * @param event
 * @return
 */
static inline std::ostream& operator<<(std::ostream& stream, const GestureEvent& event) {
    return stream << "{ #" << event.seq << " ID: " << event.getRegionID() << ", " << event.getDeviceID() << " Detail: "<<event.detail<<
        "Flags: " >> event.flags << ", Time: " << event.time << "}";
}


static std::vector<GestureBinding*> gestureBindings;
std::vector<GestureBinding*>& getGestureBindings() {
    return gestureBindings;
}

bool GestureFlags::matchesFlags(const GestureFlags& flags) const {
    return avgSqDistance.contains(flags.avgSqDistance) &&
        count.contains(flags.count) &&
        duration.contains(flags.duration) &&
        fingers.contains(flags.fingers) &&
        totalSqDistance.contains(flags.totalSqDistance) &&
        (getMask() == 0 || (getMask() & flags.getMask()) == flags.getMask()) &&
        reflectionMask == flags.reflectionMask;
}

bool GestureBinding::matches(const GestureEvent& event) const {
    if( (!regionID || regionID == event.getRegionID()) &&
        (!deviceID || deviceID == event.getDeviceID()) &&
        gestureFlags.matchesFlags(event.flags))
        if((!getDetails().size() || getDetails() == event.getDetail()))
            return 1;
    return 0;
}

void triggerGestureBinding(GestureEvent&event){
    for(auto* binding: getGestureBindings())
        if(binding->matches(event)){
            DEBUG("Match found");
            binding->func();
        }
}

void (*onGestureEvent)(GestureEvent&event) = triggerGestureBinding;
void registerOnGestureEvent(void (*newOnGestureEvent)(GestureEvent& event)) {
    onGestureEvent = newOnGestureEvent;
}
bool isNextGestureReady() {
    return getGestureQueueSize();
}
uint32_t getGestureQueueSize() {
    return (reflectionEvent?1:0) + gestureQueue.size() + touchQueue.size();
}

GestureEvent* getNextGesture() {

    if(!isNextGestureReady())
        return NULL;
    if(reflectionEvent) {
        auto*temp =reflectionEvent ;
        reflectionEvent = NULL;
        return temp;
    }
    GestureEvent* event;
    if(touchQueue.empty() || !gestureQueue.empty() && *gestureQueue.front() < *touchQueue.front()) {
        event = gestureQueue.front();
        gestureQueue.pop_front();
        event->wait();
        while(!gestureQueue.empty()) {
            DEBUG("Trying to combine gesture with " << *gestureQueue.front());
            auto delta = gestureQueue.front()->time - event->time;
            if(*event == *gestureQueue.front() && delta <  GESTURE_MERGE_DELAY_TIME) {
                auto* temp = gestureQueue.front();
                gestureQueue.pop_front();
                DEBUG("Combining event " << *event << " with " << *temp << " Delta " << delta);
                temp->wait();
                event->flags.count += 1;
                delete temp;
                continue;
            }
            DEBUG("Did not combine events Delta " << delta);
            break;
        }
    }
    else {
        event = touchQueue.front();
        touchQueue.pop_front();
    }
    if(event->flags.reflectionMask) {
        reflectionEvent = new GestureEvent(*event);
        if(reflectionEvent->flags.reflectionMask == Rotate90Mask)
            reflectionEvent->flags.reflectionMask = Rotate270Mask;
        else if(reflectionEvent->flags.reflectionMask == Rotate270Mask)
            reflectionEvent->flags.reflectionMask = Rotate90Mask;
        reflectionEvent->detail = transformGestureDetail(event->detail, reflectionEvent->flags.reflectionMask);
    }
    assert(event);
    DEBUG(*event);
    return event;
}
GestureEvent* waitForNextGesture() {
    while(!isNextGestureReady()){
        justWait();
    }
    return getNextGesture();
}

static inline unsigned int getTime() {
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000 + start.tv_usec * 1e-3;
}
void GestureEvent::wait() const {
    uint32_t delta = getTime() - time;
    if(delta < GESTURE_MERGE_DELAY_TIME)
        std::this_thread::sleep_for(std::chrono::milliseconds(GESTURE_MERGE_DELAY_TIME - delta));
}
