#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "event.h"
#include "gestures-private.h"
#include "touch.h"

#define SQUARE(X) ((X)*(X))
#define SQ_DIST(P1,P2) SQUARE(P1.x-P2.x)+SQUARE(P1.y-P2.y)

/// Used to determine if a line as a positive, 0 or negative slope
#define SIGN_THRESHOLD(X) ((X)>.5?1:(X)>=-.5?0:-1)

GestureType getLineType(const GesturePoint start, const GesturePoint end) {
    double dx = end.x - start.x, dy = end.y - start.y;
    double sum = sqrt(SQUARE(dx) + SQUARE(dy));
    int x = ((SIGN_THRESHOLD(dx / sum)));
    int y = ((SIGN_THRESHOLD(dy / sum)));
    GestureType type = GESTURE_WEST + (2 + x) * y - (2 * x + 2) * (!y);
    assert(type < 16);
    return type;
}

struct GestureGroup;
typedef struct Gesture {
    struct Gesture* next;
    struct GestureGroup* parent;
    TouchID id;
    bool finished;
    GestureDetail info;
    GesturePoint firstPoint;
    GesturePoint firstPercentPoint;
    GesturePoint lastPoint;
    GesturePoint lastPercentPoint;
    GestureType lastDir;
    int numPoints;
    uint32_t start;
    GestureFlags flags;
    bool truncated;
} Gesture ;

GestureType getGestureType(const GestureDetail detail, int N) {
    return detail[N] ;
}

int getNumOfTypes(const GestureDetail detail) {
    for(int i = 0; i < MAX_GESTURE_DETAIL_SIZE; i++)
        if(getGestureType(detail, i) == GESTURE_NONE)
            return i;
    return MAX_GESTURE_DETAIL_SIZE;
}

bool areDetailsEqual(const GestureDetail detail, const GestureDetail detail2) {
    return memcmp(detail, detail2, sizeof(GestureDetail)) == 0;
}


static inline void addGestureType(GestureDetail detail, GestureType type) {
    detail[getNumOfTypes(detail)] = type;
}

static inline void setGestureType(GestureDetail detail, GestureType type) {
    assert(getNumOfTypes(detail) == 0);
    detail[0] = type;
}

static inline bool addGesturePoint(Gesture* g, GesturePoint point, GesturePoint pixelPoint, bool first) {
    if(!first) {
        GesturePoint lastPoint = g->lastPoint;
        uint32_t distance = SQ_DIST(lastPoint, point);
        if(distance < THRESHOLD_SQ)
            return 0;
        g->flags.totalSqDistance = g->flags.totalSqDistance + distance;
        GestureType dir = getLineType(g->lastPoint, point);
        if(dir != g->lastDir) {
            if(getNumOfTypes(g->info) == MAX_GESTURE_DETAIL_SIZE) {
                g->truncated = true;
                return 0;
            }
            addGestureType(g->info, dir);
            g->lastDir = dir;
        }
    }
    g->numPoints++;
    g->lastPoint = point;
    g->lastPercentPoint = pixelPoint;
    return 1;
}

typedef struct GestureGroup {
    struct GestureGroup* next;
    GestureGroupID id;
    Gesture root;
    int activeCount ;
    int finishedCount ;
    char sysName[DEVICE_NAME_LEN];
    char name[DEVICE_NAME_LEN];
} GestureGroup ;
static GestureGroup root;

ProductID __attribute__((weak)) generateIDHighBits(const TouchEvent* touchEvent __attribute__((unused))) {
    return 0;
}
static GestureGroupID generateID(const TouchEvent* event) {
    int regionID = generateIDHighBits(event);
    return (((GestureGroupID)regionID) << 32L) | ((GestureGroupID)event->id)  ;
}

static TouchID generateTouchID(ProductID id, uint32_t seat) {
    return ((uint64_t)id) << 32L | seat;
}

static GestureGroup* addGroup(GestureGroupID id, const char* sysName, const char* name) {
    GestureGroup* newNode = calloc(1, sizeof(GestureGroup));
    newNode->id = id;
    strncpy(newNode->sysName, sysName, DEVICE_NAME_LEN - 1);
    strncpy(newNode->name, name, DEVICE_NAME_LEN - 1);
    newNode->next = root.next;
    root.next = newNode;
    return newNode;
}

static void removeGroup(GestureGroup* group) {
    for(GestureGroup* node = &root; node; node = node->next)
        if(node->next == group) {
            node->next = group->next;
            for(Gesture* gesture = group->root.next; gesture;) {
                Gesture* temp = gesture->next;
                free(gesture);
                gesture = temp;
            }
            free(group);
        }
}

static Gesture* createGesture(GestureGroup* group, TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = calloc(1, sizeof(Gesture));
    gesture->id = id;
    gesture->parent = group;
    gesture->next = group->root.next;
    group->root.next = gesture;
    gesture->firstPoint = event.point;
    gesture->firstPercentPoint = event.pointPercent;
    gesture->start = event.time;
    addGesturePoint(gesture, event.point, event.pointPercent, 1);
    group->activeCount++;
    return gesture;
}

static int finishGesture(Gesture* gesture) {
    gesture->finished = true;
    gesture->parent->finishedCount++;
    return --gesture->parent->activeCount;
}

static void removeGesture(Gesture* gesture) {
    for(Gesture* node = &gesture->parent->root; node->next; node = node->next) {
        if(node->next == gesture) {
            node->next = gesture->next;
            free(gesture);
            return;
        }
    }
    assert(0);
}

static GestureGroup* findGroup(GestureGroupID id) {
    for(GestureGroup* node = root.next; node; node  = node->next) {
        if(node->id == id)
            return node;
    }
    return NULL;
}
static Gesture* findGesture(TouchID id) {
    for(GestureGroup* group = root.next; group; group  = group->next)
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next)
            if(gesture->id == id && !gesture->finished)
                return gesture;
    return NULL;
}

void enqueueEvent(GestureEvent* event);



GestureType getMirroredXDirection(GestureType d) {
    return ((~(d - GESTURE_EAST) + 5) % 8) + GESTURE_EAST;
}

GestureType getMirroredYDirection(GestureType d) {
    return ((~(d - GESTURE_EAST) + 1) % 8) + GESTURE_EAST;
}

GestureType getRot90Direction(GestureType d) {
    return ((d - GESTURE_EAST   + 2) % 8) + GESTURE_EAST   ;
}

GestureType getRot270Direction(GestureType d) {
    return ((d - GESTURE_EAST  + 6) % 8) + GESTURE_EAST  ;
}
GestureType getOppositeDirection(GestureType d) {
    return ((d - GESTURE_EAST + 4) % 8) + GESTURE_EAST ;
}

GestureType* transformGestureDetail(GestureDetail detail, TransformMasks mask) {
    if(mask) {
        for(int i = 0; i < getNumOfTypes(detail); i++)
            detail[i] = getReflection(mask, getGestureType(detail, i));
    }
    return detail;
}

const char* getGestureMaskString(GestureMask mask) {
    switch(mask) {
        case GestureEndMask:
            return "GestureEndMask";
        case TouchEndMask:
            return "TouchEndMask";
        case TouchStartMask:
            return "TouchStartMask";
        case TouchHoldMask:
            return "TouchHoldMask";
        case TouchMotionMask:
            return "TouchMotionMask";
        case TouchCancelMask:
            return "TouchCancelMask";
    }
    return "UNKNOWN";
}
const char* getGestureTypeString(GestureType t) {
    switch(t) {
        case GESTURE_NONE:
            return "NONE";
        case GESTURE_NORTH_WEST:
            return "NORTH_WEST";
        case GESTURE_WEST:
            return "WEST";
        case GESTURE_SOUTH_WEST:
            return "SOUTH_WEST";
        case GESTURE_NORTH:
            return "NORTH";
        case GESTURE_SOUTH:
            return "SOUTH";
        case GESTURE_NORTH_EAST:
            return "NORTH_EAST";
        case GESTURE_EAST:
            return "EAST";
        case GESTURE_SOUTH_EAST:
            return "SOUTH_EAST";
        case GESTURE_PINCH:
            return "PINCH";
        case GESTURE_PINCH_OUT:
            return "PINCH_OUT";
        case GESTURE_TAP:
            return "TAP";
        case GESTURE_TOO_LARGE:
            return "TOO_LARGE";
        default:
        case GESTURE_UNKNOWN:
            return "UNKNOWN";
    }
}

bool generatePinchEvent(GestureEvent* gestureEvent, GestureGroup* group) {
    if(gestureEvent->flags.fingers > 1) {
        GesturePoint avgEnd = {0, 0};
        GesturePoint avgStart = {0, 0};
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next) {
            ADD_POINT(avgStart, gesture->firstPoint);
            ADD_POINT(avgEnd, gesture->lastPoint);
        }
        DIVIDE_POINT(avgEnd, gestureEvent->flags.fingers);
        DIVIDE_POINT(avgStart, gestureEvent->flags.fingers);
        Gesture* ref = NULL;
        double refDistance = -1;
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next) {
            double dist = SQ_DIST(gesture->lastPoint, avgEnd) + SQ_DIST(gesture->firstPoint, avgStart);
            if(dist > refDistance) {
                refDistance = dist;
                ref = gesture;
            }
        }
        assert(ref);
        double avgStartDis = 0;
        double avgEndDis = 0;
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next) {
            avgEndDis += SQ_DIST(gesture->lastPoint, ref->lastPoint);
            avgStartDis += SQ_DIST(gesture->firstPoint, ref->firstPoint);
        }
        avgEndDis /= (gestureEvent->flags.fingers - 1);
        avgStartDis /= (gestureEvent->flags.fingers - 1);
        double percentDiff = (avgStartDis - avgEndDis) * 2 / (avgStartDis + avgEndDis);
        if(percentDiff > PINCH_THRESHOLD_PERCENT)
            setGestureType(gestureEvent->detail,  GESTURE_PINCH);
        else if(percentDiff < -PINCH_THRESHOLD_PERCENT)
            setGestureType(gestureEvent->detail,  GESTURE_PINCH_OUT);
        else return 0;
        return 1;
    }
    return 0;
}

bool setReflectionMask(GestureEvent* gestureEvent, GestureGroup* group) {
    Gesture* gesture = group->root.next;
    uint32_t sameCount = 0;
    const TransformMasks reflectionMasks[] = {MirroredMask, MirroredXMask, MirroredYMask, Rotate90Mask};
    uint32_t reflectionCounts[LEN(reflectionMasks)] = {0};
    for(Gesture* node = gesture; node; node = node->next)
        if(getNumOfTypes(node->info) != getNumOfTypes(gesture->info)) {
            break;
        }
        else if(areDetailsEqual(node->info, gesture->info)) {
            sameCount++;
        }
        else {
            for(int i = 0; i < getNumOfTypes(gesture->info); i++) {
                reflectionCounts[0] += getOppositeDirection(getGestureType(gesture->info, i)) == getGestureType(node->info, i);
                reflectionCounts[1] += getMirroredXDirection(getGestureType(gesture->info, i)) == getGestureType(node->info, i);
                reflectionCounts[2] += getMirroredYDirection(getGestureType(gesture->info, i)) == getGestureType(node->info, i);
                reflectionCounts[3] += getRot90Direction(getGestureType(gesture->info, i)) == getGestureType(node->info, i) ||
                    getRot270Direction(getGestureType(gesture->info, i)) == getGestureType(node->info, i);
            }
        }
    if(sameCount == gestureEvent->flags.fingers) {
        memcpy(gestureEvent->detail, gesture->info, sizeof(GestureDetail));
        return 1;
    }
    for(uint32_t i = 0; i < LEN(reflectionMasks); i++) {
        if(sameCount + reflectionCounts[i] / getNumOfTypes(gesture->info) == gestureEvent->flags.fingers) {
            gestureEvent->flags.reflectionMask = reflectionMasks[i];
            memcpy(gestureEvent->detail, gesture->info, sizeof(GestureDetail));
            return 1;
        }
    }
    return 0;
}
void setFlags(Gesture* g, GestureEvent* event) {
    g->flags.avgSqDisplacement = SQ_DIST(g->firstPoint, g->lastPoint);
    g->flags.avgSqDistance = g->flags.totalSqDistance;
    g->flags.duration = event->time - g->start;
    event->flags.totalSqDistance = g->flags.totalSqDistance;
    event->flags.avgSqDisplacement  = g->flags.avgSqDisplacement;
    event->flags.avgSqDistance = g->flags.avgSqDistance;
    event->flags.duration = event->time - g->start;
}

void combineFlags(GestureGroup* group, GestureEvent* event) {
    uint32_t minStartTime = -1;
    for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next) {
        event->flags.avgSqDisplacement += gesture->flags.avgSqDisplacement;
        event->flags.avgSqDistance += gesture->flags.avgSqDistance;
        event->flags.totalSqDistance += gesture->flags.totalSqDistance;
        if(gesture->start < minStartTime)
            minStartTime  = gesture->start;
    }
    event->flags.avgSqDisplacement /= event->flags.fingers;
    event->flags.avgSqDistance /= event->flags.fingers;
    event->flags.duration = event->time - minStartTime;
}

static uint32_t gestureEventSeqCounter;
GestureEvent* generateGestureEvent(Gesture* g, uint32_t mask, uint32_t time) {
    assert(g);
    assert(g->parent);
    GestureGroup* group = g->parent;
    assert(group);
    GestureEvent* gestureEvent = malloc(sizeof(GestureEvent));
    *gestureEvent = (GestureEvent) {
        .seq = ++gestureEventSeqCounter,
        .id = group->id,
        .time = time,
        .endPoint = g->lastPoint,
        .endPercentPoint = g->lastPercentPoint,
        .flags = {
            .mask = mask,
            .fingers = group->activeCount + group->finishedCount
        }
    };
    if(mask == GestureEndMask) {
        combineFlags(group, gestureEvent);
        if(setReflectionMask(gestureEvent, group)) {}
        else if(generatePinchEvent(gestureEvent, group)) {}
        else {
            setGestureType(gestureEvent->detail, GESTURE_UNKNOWN);
        }
        return gestureEvent;
    }
    else
        setFlags(g, gestureEvent);
    if(g->numPoints == 1)
        setGestureType(gestureEvent->detail, GESTURE_TAP);
    else
        memcpy(gestureEvent->detail, g->info, sizeof(GestureDetail));
    return gestureEvent;
}

void startGesture(const TouchEvent event, const char* sysName, const char* name) {
    GestureGroupID gestureGroupID = generateID(&event);
    GestureGroup* group = findGroup(gestureGroupID);
    if(!group) {
        group = addGroup(event.id, sysName, name);
    }
    assert(group == findGroup(gestureGroupID));
    Gesture* gesture = createGesture(group, event);
    assert(gesture == findGesture(generateTouchID(event.id, event.seat)));
    enqueueEvent(generateGestureEvent(gesture, TouchStartMask, event.time));
}

void continueGesture(const TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = findGesture(id);
    if(gesture) {
        if(!gesture->truncated) {
            bool newGesturePoint = addGesturePoint(gesture, event.point, event.pointPercent, 0);
            enqueueEvent(generateGestureEvent(gesture, newGesturePoint ? TouchMotionMask : TouchHoldMask, event.time));
        }
    }
}

void cancelGesture(const TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = findGesture(id);
    if(gesture) {
        enqueueEvent(generateGestureEvent(gesture, TouchCancelMask, event.time));
        if(gesture->parent->activeCount == 1)
            removeGroup(gesture->parent);
        else
            removeGesture(gesture);
    }
}

void endGesture(const TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = findGesture(id);
    if(gesture) {
        assert(gesture->numPoints);
        if(gesture->numPoints == 1) {
            gesture->info[0] = GESTURE_TAP;
        }
        enqueueEvent(generateGestureEvent(gesture, TouchEndMask, event.time));
        assert(gesture->parent->activeCount);
        if(finishGesture(gesture) == 0) {
            enqueueEvent(generateGestureEvent(gesture, GestureEndMask, event.time));
            removeGroup(gesture->parent);
        }
    }
}
