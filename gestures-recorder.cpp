#include <assert.h>
#include <math.h>

#include "gestures.h"
#include "gestures-helper.h"
#include "gestures-private.h"

#define SQUARE(X) ((X)*(X))
#define SQ_DIST(P1,P2) SQUARE(P1.x-P2.x)+SQUARE(P1.y-P2.y)

/// Used to determine if a line as a positive, 0 or negative slope
#define SIGN_THRESHOLD(X) ((X)>.5?1:(X)>=-.5?0:-1)

struct GestureGroup;
struct Gesture {
    Gesture* next;
    GestureGroup* parent;
    TouchID id;
    bool finished;
    std::vector<GesturePoint> points;
    GestureDetail info;
    GesturePoint lastPoint;
    GesturePoint lastPixelPoint;
    uint32_t start;
    GestureFlags flags;
    std::vector<GesturePoint>& getGesturePoints() {return points;}
    bool addGesturePoint(GesturePoint point, bool first = 0);
};

bool Gesture::addGesturePoint(GesturePoint point, bool first) {
    if(!first) {
        GesturePoint lastPoint = getGesturePoints().back();
        auto distance = SQ_DIST(lastPoint, point);
        if(distance < THRESHOLD_SQ)
            return 0;
        flags.totalSqDistance = flags.totalSqDistance + distance;
    }
    getGesturePoints().push_back(point);
    return 1;
}

struct GestureGroup {
    GestureGroup* next;
    GestureGroupID id;
    Gesture root;
    int activeCount = 0;
    int finishedCount = 0;
    std::string sysName = "";
    std::string name = "";
};
static GestureGroup root;

ProductID __attribute__((weak)) generateIDHighBits(ProductID id __attribute__((unused)), GesturePoint startingGesturePoint __attribute__((unused))) {
    return 0;
}
static GestureGroupID generateID(ProductID id, GesturePoint startingGesturePoint) {
    int regionID = generateIDHighBits(id, startingGesturePoint);
    return (((GestureGroupID)regionID) << 32L) | ((GestureGroupID)id)  ;
}

static TouchID generateTouchID(ProductID id, uint32_t seat) {
    return ((uint64_t)id) << 32L | seat;
}

static GestureGroup* addGroup (GestureGroupID id, std::string sysName, std::string name) {
    GestureGroup* newNode = new GestureGroup{
        .id = id,
        .sysName = sysName,
        .name = name,
    };
    newNode->next = root.next;
    root.next = newNode;
    return newNode;

}

static void removeGroup (GestureGroup*group) {
    for(GestureGroup* node = &root; node; node = node->next)
        if(node->next == group){
            node->next = group->next;
            for(Gesture* gesture = group->root.next; gesture; ) {
                Gesture*temp = gesture->next;
                delete gesture;
                gesture = temp;
            }
            delete group;
        }
}

static Gesture* createGesture(GestureGroup*group, TouchEvent event){

    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = new Gesture();
    gesture->id = id;
    gesture->parent = group;
    gesture->next=group->root.next;
    group->root.next= gesture;
    gesture->lastPoint = event.point;
    gesture->lastPixelPoint = event.pointPixel;
    gesture->start = event.time;
    gesture->addGesturePoint(event.point, 1);

    group->activeCount++;
    return gesture;
}

static int finishGesture(Gesture*gesture) {
    gesture->finished = true;
    gesture->parent->finishedCount++;
    return --gesture->parent->activeCount;
}

static void removeGesture(Gesture*gesture){
    for(Gesture* node = &gesture->parent->root; node->next; node = node->next){
        if(node->next == gesture) {
            node->next = gesture->next;
            delete gesture;
            return;
        }
    }
    assert(0);
}

static GestureGroup* findGroup(GestureGroupID id){
    for(GestureGroup* node = root.next; node; node  = node->next){
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

GestureEvent* enqueueEvent(GestureEvent* event);


GestureType getOppositeDirection(GestureType d) {
    return (GestureType)(GESTURE_SOUTH_EAST - d + GESTURE_NORTH_WEST );
}
GestureType getMirroredXDirection(GestureType d) {
    return (GestureType)(d - ((d - GESTURE_NORTH_WEST) / 4 - 1) * (GESTURE_NORTH_EAST - GESTURE_NORTH_WEST ));
}
GestureType getMirroredYDirection(GestureType d) {
    return (GestureType)((d % 4 - 1) * (-2) + d);
}

GestureType getRot90Direction(GestureType d) {
    int sign = ((d - GESTURE_NORTH_WEST) / (GESTURE_TAP - GESTURE_NORTH_WEST) * 2) - 1;
    int a = ((d- GESTURE_NORTH_WEST) / 4 - 1);
    int odd = (d % 2);
    int b = (a * (2 + odd * 3));
    int c = (!a * -3 * sign);
    int e = (d - GESTURE_NORTH_WEST - a * 3 == 5) * a * 6;
    GestureType result = (GestureType)(d - b - c  - e  );
    return result;
}

GestureType getRot270Direction(GestureType d) {
    return getRot90Direction(getOppositeDirection(d));
}

GestureDetail transformGestureDetail(const GestureDetail& detail, TransformMasks mask) {
    if(mask) {
        GestureDetail mirror = {};
        for(int i = 0; i< detail.size(); i++)
            mirror.push_back(getReflection(mask, detail[i]));
        return GestureDetail(mirror);
    }
    return detail;
}

const char* getGestureTypeString(GestureType t) {
    switch(t) {
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
        default:
            return "UNKNOWN";
    }
}
static double getRSquared(const std::vector<GesturePoint> points, int start = 0, int num = 0) {
    double sumX = 0, sumY = 0, sumXY = 0;
    double sumX2 = 0, sumY2 = 0;
    if(!num)
        num = points.size() - start;
    for(int i = start; i < num; i++) {
        sumX += points[i].x;
        sumY += points[i].y;
        sumXY += points[i].x * points[i].y;
        sumX2 += SQUARE(points[i].x);
        sumY2 += SQUARE(points[i].y);
    }
    double r2 = (1e-6 + SQUARE(num * sumXY - sumX * sumY)) / (1e-6 + (num * sumX2 - SQUARE(sumX)) * (num * sumY2 - SQUARE(
                    sumY)));
    return r2;
}
GestureType getLineType(GesturePoint start, GesturePoint end) {
    double dx = end.x - start.x, dy = end.y - start.y;
    double sum = sqrt(SQUARE(dx) + SQUARE(dy));
    return (GestureType)((((SIGN_THRESHOLD(dx / sum) + 1) << 2) | (SIGN_THRESHOLD(dy / sum) + 1)) + GESTURE_NORTH_WEST );
}
void detectSubLines(Gesture* gesture) {
    auto& info = gesture->info;
    std::vector<GesturePoint>& points = gesture->points;
    GestureType lastDirection = GESTURE_TAP;
    for(uint32_t i = 1; i < points.size(); i++) {
        auto dir = getLineType(points[i - 1], points[i]);
        if(dir != GESTURE_TAP)
            if(lastDirection != dir){
                info.push_back(dir);
                lastDirection = dir;
            }
    }
}

bool generatePinchEvent(GestureEvent* gestureEvent, GestureGroup* group) {
    if(gestureEvent->flags.fingers > 1 && gestureEvent->flags.avgSqDisplacement > THRESHOLD_SQ) {
        GesturePoint avgEnd = {0, 0};
        GesturePoint avgStart = {0, 0};
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next){
            avgEnd += gesture->getGesturePoints().back();
            avgStart += gesture->getGesturePoints()[0];
        }
        avgEnd /= gestureEvent->flags.fingers;
        avgStart /= gestureEvent->flags.fingers;
        double avgStartDis = 0;
        double avgEndDis = 0;
        for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next){
            avgEndDis += SQ_DIST(gesture->getGesturePoints().back(), avgEnd);;
            avgStartDis += SQ_DIST(gesture->getGesturePoints()[0], avgStart);;
        }
        avgEndDis /= gestureEvent->flags.fingers;
        avgStartDis /=  gestureEvent->flags.fingers;
        if(avgStartDis < PINCH_THRESHOLD && avgEndDis > PINCH_THRESHOLD)
            gestureEvent->detail = {GESTURE_PINCH_OUT};
        else if(avgStartDis > PINCH_THRESHOLD && avgEndDis < PINCH_THRESHOLD)
            gestureEvent->detail = {GESTURE_PINCH};
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

        for(Gesture* node= gesture; node; node = node->next)
            if(node->info.size() != gesture->info.size()){
                break;
            }
            else if(node->info == gesture->info){
                sameCount++;
            }
            else {
                for(auto i = 0; i < gesture->info.size(); i++) {
                    reflectionCounts[0] += getOppositeDirection(gesture->info[i]) == node->info[i];
                    reflectionCounts[1] += getMirroredXDirection(gesture->info[i]) == node->info[i];
                    reflectionCounts[2] += getMirroredYDirection(gesture->info[i]) == node->info[i];
                    reflectionCounts[3] += getRot90Direction(gesture->info[i]) == node->info[i] ||
                        getRot270Direction(gesture->info[i]) == node->info[i];
                }
            }
    if(sameCount == gestureEvent->flags.fingers) {
        gestureEvent->detail = gesture->info;
        return 1;
    }
    for(uint32_t i = 0; i < LEN(reflectionMasks); i++) {
        if(sameCount + reflectionCounts[i] / gesture->info.size() == gestureEvent->flags.fingers) {
            gestureEvent->flags.reflectionMask = reflectionMasks[i];
            gestureEvent->detail = gesture->info;
            return 1;
        }
    }
    return 0;
}
void setFlags(Gesture* g, GestureEvent* event) {
    g->flags.avgSqDisplacement = g->getGesturePoints().size() > 1 ?
        SQ_DIST(g->getGesturePoints()[g->getGesturePoints().size() - 2], g->getGesturePoints().back()) :
        0;
    g->flags.avgSqDistance = g->flags.totalSqDistance;
    g->flags.duration = event->time - g->start;
    event->flags.totalSqDistance = g->flags.totalSqDistance;
    event->flags.avgSqDisplacement  = g->flags.avgSqDisplacement;
    event->flags.avgSqDistance = g->flags.avgSqDistance;
    event->flags.duration = event->time - g->start;
}

void combineFlags(GestureGroup* group, GestureEvent* event) {
    uint32_t minStartTime = -1;

    for(Gesture* gesture = group->root.next; gesture; gesture = gesture->next){
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
GestureEvent* generateGestureEvent(Gesture* g, uint32_t mask, uint32_t time) {
    assert(g);
    assert(g->parent);
    GestureGroup* group = g->parent;
    assert(group);
    GestureEvent* gestureEvent = new GestureEvent({
        .id = group->id,
        .time = time,
        .endPoint = g->lastPoint,
        .endPixelPoint = g->lastPixelPoint,
    });
    gestureEvent->flags.mask = mask;
    gestureEvent->flags.fingers = group->activeCount + group->finishedCount;

    if(mask == GestureEndMask) {
        combineFlags(group, gestureEvent);
        if(setReflectionMask(gestureEvent, group)) {}
        else if(generatePinchEvent(gestureEvent, group)) {}
        else {
            gestureEvent->detail = {GESTURE_UNKNOWN};
        }
        return gestureEvent;
    }
    else
        setFlags(g, gestureEvent);
    if(mask == TouchEndMask) {
        assert(g->info.size());
        gestureEvent->detail = g->info;
    }
    else if(g->getGesturePoints().size() == 1)
        gestureEvent->detail = {GESTURE_TAP};
    else
        gestureEvent->detail = {getLineType(g->getGesturePoints()[g->getGesturePoints().size() - 2], g->getGesturePoints().back())};
    return gestureEvent;
}

void startGesture(TouchEvent event, std::string sysName, std::string name) {
    GestureGroupID gestureGroupID = generateID(event.id, event.point);
    GestureGroup* group = findGroup(gestureGroupID);
    if(!group) {
        group = addGroup(event.id, sysName, name);
    }
    assert(group == findGroup(gestureGroupID));
    Gesture* gesture = createGesture(group, event);
    assert(gesture == findGesture(generateTouchID(event.id, event.seat)));

    enqueueEvent(generateGestureEvent(gesture, TouchStartMask, event.time));
}

void continueGesture(TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = findGesture(id);
    if(gesture) {
        bool newGesturePoint = gesture->addGesturePoint(event.point, 0);
        gesture->lastPoint = event.point;
        gesture->lastPixelPoint = event.pointPixel;
        enqueueEvent(generateGestureEvent(gesture, newGesturePoint ? TouchMotionMask : TouchHoldMask, event.time));
    }
}

void cancelGesture(TouchEvent event) {
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

void endGesture(TouchEvent event) {
    TouchID id = generateTouchID(event.id, event.seat);
    Gesture* gesture = findGesture(id);
    if(gesture) {
        assert(gesture->info.size() == 0 && gesture->getGesturePoints().size());
        if(gesture->getGesturePoints().size() == 1)
            gesture->info.push_back(GESTURE_TAP);
        else if(getRSquared(gesture->points) > R_SQUARED_THRESHOLD) {
            gesture->info.push_back(getLineType(gesture->getGesturePoints()[0], gesture->getGesturePoints().back()));
        }
        else {
            detectSubLines(gesture);
        }
        enqueueEvent(generateGestureEvent(gesture, TouchEndMask, event.time));
        assert(gesture->parent->activeCount);
        if(finishGesture(gesture) == 0) {
            enqueueEvent(generateGestureEvent(gesture, GestureEndMask, event.time));
            removeGroup(gesture->parent);
        }
    }
}
