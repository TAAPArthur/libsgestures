#include <assert.h>
#include <stdlib.h>
#include <scutest/tester.h>

#include "event.h"
#include "gestures-private.h"
#include "gestures.h"
#include "touch.h"

#define NULL_POINT ((GesturePoint){-1, -1})

GestureType getLineType(const GesturePoint start, const GesturePoint end);
typedef struct {
    void(*const func)();
    GestureBindingArg arg;
} GestureBinding;
void triggerGestureBinding(GestureBinding* bindings, int N, const GestureEvent* event) {
    for(int i = 0; i < N; i++)
        if(matchesGestureEvent(&bindings[i].arg, event))
            bindings[i].func();
}


static int SCALE_FACTOR = 2 * THRESHOLD_SQ;

SCUTEST_ITER(gesture_types_string, GESTURE_UNKNOWN + 1) {
    getGestureTypeString((GestureType)_i);
}
TransformMasks MASKS[] = {MirroredXMask, MirroredYMask, MirroredMask, Rotate90Mask};
const struct GestureChecker {
    GestureType type;
    GestureType typeMirrorX;
    GestureType typeMirrorY;
    GestureType typeOpposite;
    GestureType rot90;
} gestureTypeTuples[] = {
    {GESTURE_NORTH_WEST, GESTURE_NORTH_EAST, GESTURE_SOUTH_WEST, GESTURE_SOUTH_EAST, GESTURE_SOUTH_WEST},
    {GESTURE_SOUTH, GESTURE_SOUTH, GESTURE_NORTH, GESTURE_NORTH, GESTURE_EAST},
    {GESTURE_SOUTH_EAST, GESTURE_SOUTH_WEST, GESTURE_NORTH_EAST, GESTURE_NORTH_WEST, GESTURE_NORTH_EAST},
};
SCUTEST(validate_dir_simple) {
    assert(GESTURE_EAST == getMirroredXDirection(GESTURE_WEST));
    assert(GESTURE_WEST == getMirroredXDirection(GESTURE_EAST));
    assert(GESTURE_WEST == getMirroredYDirection(GESTURE_WEST));
    assert(GESTURE_EAST == getMirroredYDirection(GESTURE_EAST));
}
SCUTEST_ITER(validate_dir, LEN(gestureTypeTuples)) {
    struct GestureChecker values = gestureTypeTuples[_i];
    assert(values.type == getOppositeDirection(values.typeOpposite));
    assert(values.type == getOppositeDirection(getOppositeDirection(values.type)));
    assert(values.type == getRot270Direction(values.rot90));
    assert(values.type == getRot90Direction(getRot90Direction(values.typeOpposite)));
    assert(values.type == getRot90Direction(getRot270Direction(values.type)));
    assert(values.type == getRot270Direction(getRot270Direction(values.typeOpposite)));
    assert(values.type == getMirroredXDirection(values.typeMirrorX));
    assert(values.type == getMirroredXDirection(getMirroredXDirection(values.type)));
    assert(values.type == getMirroredYDirection(values.typeMirrorY));
    assert(values.type == getMirroredYDirection(getMirroredYDirection(values.type)));
}

GesturePoint lineDelta[] = {
    [GESTURE_TAP] = {0, 0},
    [GESTURE_EAST] = {1, 0},
    [GESTURE_NORTH_EAST] = {1, -1},
    [GESTURE_NORTH] = {0, -1},
    [GESTURE_NORTH_WEST] = {-1, -1},
    [GESTURE_WEST] = {-1, 0},
    [GESTURE_SOUTH_WEST] = {-1, 1},
    [GESTURE_SOUTH] = {0, 1},
    [GESTURE_SOUTH_EAST] = {1, 1},
};

SCUTEST_ITER(validate_line_type, LEN(lineDelta)) {
    GesturePoint p = lineDelta[_i];
    if(p.x && p.y) {
        assert(getLineType((GesturePoint) {0, 0}, p) == _i);
    }
}

static int FAKE_DEVICE_ID = 0;
static unsigned int timeCounter = 0;
static void continueGestureWrapper(ProductID id, int32_t seat, GesturePoint point) {
    continueGesture((TouchEvent) {id, seat, point, point, timeCounter++});
}
static void startGestureWrapper(ProductID id, int32_t seat, GesturePoint point) {
    startGesture((TouchEvent) {id, seat, point, point, timeCounter++}, "", "");
}
static void endGestureWrapper(ProductID id, int32_t seat) {
    endGesture((TouchEvent) {id, seat, {}, {}, timeCounter++});
}
void cancelGestureWrapper(ProductID id, int32_t seat) {
    cancelGesture((TouchEvent) {id, seat, {}, {}, timeCounter++});
}

static inline GesturePoint multiplePoint(GesturePoint P, int N) {
    return (GesturePoint) {P.x *= N, P.y *= N};
}
static void startGestureWithSteps(const GesturePoint* points, int N, int seat, int steps) {
    startGestureWrapper(FAKE_DEVICE_ID, seat, multiplePoint(points[0], SCALE_FACTOR));
    for(int i = 1; i < N; i++) {
        if(memcmp(&points[i], &NULL_POINT, sizeof(GesturePoint)) == 0)
            break;
        if(steps) {
            GesturePoint delta;
            delta.x = points[i].x - points[i - 1].x;
            delta.y = points[i].y - points[i - 1].y;
            DIVIDE_POINT(delta, steps);
            GesturePoint p = points[i - 1];
            for(int s = 0; s < steps - 1; s++) {
                ADD_POINT(p, delta);
                continueGestureWrapper(FAKE_DEVICE_ID, seat, multiplePoint(p, SCALE_FACTOR));
            }
        }
        continueGestureWrapper(FAKE_DEVICE_ID, seat, multiplePoint(points[i], SCALE_FACTOR));
    }
}

static void startGestureWithPoints(const GesturePoint* points, int N, int seat) {
    startGestureWithSteps(points, N, seat, 0);
}

static void startGestureTap(int seat) {
    GesturePoint point = {0, 0};
    startGestureWithPoints(&point, 1, seat);
}
static void endGestureHelper(int fingers) {
    assert(fingers);
    for(int n = 0; n < fingers; n++)
        endGestureWrapper(FAKE_DEVICE_ID, n);
}

static void listenForAllGestures() {
    listenForGestureEvents(-1);
}
SCUTEST_SET_ENV(listenForAllGestures, NULL);
SCUTEST(start_end_gesture) {
    startGestureTap(0);
    endGestureHelper(1);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchStartMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchEndMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == GestureEndMask);
    assert(!getNextGesture());
}
SCUTEST_ITER(start_continue_end_gesture_tap, 3) {
    int n = _i + 1;
    for(int i = 0; i < n; i++)
        startGestureTap(i);
    endGestureHelper(n);
    for(int i = 0; i < 2 * n + 1; i ++) {
        GestureEvent* event = getNextGesture();
        assert(event);
        assert(event->detail[0] == GESTURE_TAP);
    }
}

SCUTEST(reuse_seats) {
    listenForGestureEvents(GestureEndMask);
    startGestureTap(0);
    int n = 10;
    for(int i = 1; i < n; i++) {
        startGestureTap(1);
        endGestureWrapper(FAKE_DEVICE_ID, 1);
    }
    endGestureWrapper(FAKE_DEVICE_ID, 0);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(event->flags.fingers == n);
}

SCUTEST(reset_fingers) {
    for(int i = 0; i < 10; i++) {
        startGestureTap(0);
        endGestureHelper(1);
        for(int n = 0; n < 3; n ++) {
            GestureEvent* event = getNextGesture();
            assert(event);
            assert(event->flags.fingers == 1);
        }
    }
}

SCUTEST(many_points) {
    listenForGestureEvents(GestureEndMask);
    int steps = 10000;
    GesturePoint points[] = {{0, 0}, {steps, steps}};
    startGestureWithSteps(points, LEN(points), 0, steps);
    endGestureHelper(1);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(areDetailsEqual(event->detail, (GestureDetail) {GESTURE_SOUTH_EAST}));
}

SCUTEST(many_points_cont) {
    SCALE_FACTOR = 1;
    listenForGestureEvents(GestureEndMask);
    int steps = 100;
    GesturePoint points[] = {{0, 0}, {steps, steps}};
    startGestureWithSteps(points, LEN(points), 0, steps);
    endGestureHelper(1);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(areDetailsEqual(event->detail, (GestureDetail) {GESTURE_SOUTH_EAST}));
}
SCUTEST(many_lines) {
    listenForGestureEvents(GestureEndMask);
    int steps = 10000;
    startGestureTap(0);
    for(int i = 0 ; i < steps; i++) {
        continueGestureWrapper(FAKE_DEVICE_ID, 0, (GesturePoint) {SCALE_FACTOR, 0});
        continueGestureWrapper(FAKE_DEVICE_ID, 0, (GesturePoint) {SCALE_FACTOR, SCALE_FACTOR});
        continueGestureWrapper(FAKE_DEVICE_ID, 0, (GesturePoint) {0, SCALE_FACTOR});
        continueGestureWrapper(FAKE_DEVICE_ID, 0, (GesturePoint) {0, 0});
    }
    endGestureHelper(1);
    GestureEvent* event = getNextGesture();
    assert(event);
}


SCUTEST(cancel_reset) {
    listenForGestureEvents(TouchCancelMask | GestureEndMask);
    startGestureTap(0);
    cancelGestureWrapper(FAKE_DEVICE_ID, 0);
    startGestureTap(1);
    endGestureWrapper(FAKE_DEVICE_ID, 1);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchCancelMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == GestureEndMask);
    assert(event->flags.fingers == 1);
}

struct GestureEventChecker {
    GestureDetail detail;

    GesturePoint points[5];
} gestureEventTuples[] = {
    {{GESTURE_NORTH_WEST, GESTURE_SOUTH_WEST}, {{2, 2}, {1, 1}, {0, 2}, NULL_POINT }},
    {{GESTURE_WEST}, {{1, 1}, {0, 1}, NULL_POINT}},
    {{GESTURE_SOUTH_WEST, GESTURE_SOUTH_EAST}, {{1, 1}, {0, 2}, {1, 3}, NULL_POINT}},
    {{GESTURE_NORTH}, {{1, 1}, {1, 0}, NULL_POINT}},
    {{GESTURE_TAP}, {{1, 1}, NULL_POINT}},
    {{GESTURE_TAP}, {{1, 1}, {1, 1}, NULL_POINT}},
    {{GESTURE_SOUTH}, {{1, 1}, {1, 2}, NULL_POINT}},
    {{GESTURE_NORTH_EAST, GESTURE_NORTH_WEST}, {{2, 2}, {3, 1}, {2, 0}, NULL_POINT}},
    {{GESTURE_EAST}, {{1, 1}, {2, 1}, NULL_POINT}},
    {{GESTURE_SOUTH_EAST, GESTURE_NORTH_EAST}, {{1, 1}, {2, 2}, {3, 1}, NULL_POINT}},
    {{GESTURE_EAST}, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, NULL_POINT}},
};

static void listenForGestureEnd() {
    listenForGestureEvents(GestureEndMask);
}

SCUTEST_SET_ENV(listenForGestureEnd, NULL);
SCUTEST_ITER(validate_line_dir, LEN(gestureEventTuples) * 4) {
    int i = _i / 4;
    int fingers = _i % 2 + 1;
    bool cancel = _i % 4 > 2;
    struct GestureEventChecker values = gestureEventTuples[i];
    assert(fingers);
    if(cancel) {
        startGestureWithPoints(values.points, LEN(values.points), -1);
        cancelGestureWrapper(FAKE_DEVICE_ID, -1);
    }
    for(int n = 0; n < fingers; n++)
        startGestureWithPoints(values.points, LEN(values.points), n);
    endGestureHelper(fingers);
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(getNumOfTypes(event->detail));
    assert(areDetailsEqual(event->detail, values.detail));
    assert(event->flags.fingers == fingers);
    assert(!event->flags.reflectionMask);
}
static int getGesturePointsThatMakeLine(const GestureDetail detail, GesturePoint* points) {
    assert(getNumOfTypes(detail));
    GesturePoint p = {8, 8};
    int count = 1;
    points[0] = p;
    for(int i = 0; i < getNumOfTypes(detail); i++, count++) {
        GestureType type = getGestureType(detail, i);
        ADD_POINT(p, lineDelta[type]);
        points[i + 1] = p;
        if(type != GESTURE_TAP)
            assert(getLineType(points[i], p) == type);
    }
    return count;
}
SCUTEST_ITER(validate_masks, LEN(gestureEventTuples)*LEN(MASKS)) {
    int index = _i / LEN(MASKS);
    TransformMasks mask = MASKS[_i % LEN(MASKS)];
    struct GestureEventChecker values = gestureEventTuples[index];
    static GesturePoint points[32];
    GestureBindingArg bindingBase = {};
    memcpy((GestureType*)bindingBase.detail, values.detail, sizeof(GestureDetail));
    //GestureBinding bindingRefl = GestureBinding({values.detail}, {}, {.reflectionMask = mask}
    int N;
    N = getGesturePointsThatMakeLine(bindingBase.detail, points);
    startGestureWithPoints(points, N, 0);
    endGestureHelper(1);
    GestureEvent* event = getNextGesture();
    assert(!event->flags.reflectionMask || event->flags.reflectionMask & mask);
}
struct GestureMultiLineCheck {
    GesturePoint points[10];
    GestureDetail detail;
    int fingers;
    int steps;
} multiLines[] = {
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        .fingers = 1
    },
    {
        {{1, 1}, {2, 2}, {3, 1}, {2, 0}, {1, 1}, NULL_POINT},
        {GESTURE_SOUTH_EAST, GESTURE_NORTH_EAST, GESTURE_NORTH_WEST, GESTURE_SOUTH_WEST},
        .fingers = 1
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH},
        .fingers = 2
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        .fingers = 2
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        .fingers = 1,
        .steps = 10
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        10, 10
    },
    {
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 3}, {3, 9}, NULL_POINT},
        {GESTURE_EAST, GESTURE_SOUTH},
        10, 10
    },
};
SCUTEST_ITER(multi_lines, LEN(multiLines) * 2) {
    struct GestureMultiLineCheck values = multiLines[_i / 2];
    int step = (_i % 2) * 10;
    for(int i = 0; i < values.fingers; i++)
        startGestureWithSteps(values.points, LEN(values.points), i, step);
    endGestureHelper(values.fingers);
    GestureEvent* event = getNextGesture();
    assert(!event->flags.reflectionMask);
    assert(areDetailsEqual(event->detail, values.detail));
}
struct GenericGestureCheck {
    GesturePoint points[4][2];
    GestureDetail detail;
} genericGesture[] = {
    {
        {
            {{1, 1}, {1, 0}},
            {{1, 1}, {1, 2}},
            {{1, 1}, {2, 1}},
            {{1, 1}, {0, 1}},
        },
        {GESTURE_PINCH_OUT},
    },
    {
        {
            {{1, 0}, {1, 1}},
            {{1, 2}, {1, 1}},
            {{2, 1}, {1, 1}},
            {{0, 1}, {1, 1}},
        },
        {GESTURE_PINCH},
    },
    {
        {
            {{0, 1}, {1, 2}},
            {{2, 3}, {3, 4}},
            {{4, 5}, {5, 6}},
            {{6, 7}, {7, 0}},
        },
        {GESTURE_UNKNOWN},
    },
    {
        {
            {{0, 0}, NULL_POINT},
            {{0, 2}, NULL_POINT},
            {{2, 0}, NULL_POINT},
            {{2, 2}, {1, 1}},
        },
        {GESTURE_UNKNOWN},
    },
};
SCUTEST_ITER(generic_gesture, LEN(genericGesture)) {
    struct GenericGestureCheck values = genericGesture[_i];
    for(int i = 0; i < LEN(values.points); i++)
        startGestureWithPoints(values.points[i], LEN(values.points[i]), i);
    endGestureHelper(LEN(values.points));
    GestureEvent* event = getNextGesture();
    assert(!event->flags.reflectionMask);
    assert(areDetailsEqual(event->detail, values.detail));
}

struct GenericGestureBindingCheck {
    GesturePoint points[2][3];
    GestureBindingArg bindings[2];
} genericGestureBinding[] = {
    {
        {
            {{1, 1}, {2, 2}, NULL_POINT},
            {{2, 2}, {1, 1}, NULL_POINT},
        },
        {
            {{GESTURE_NORTH_WEST}, {.fingers = 2, .reflectionMask = MirroredMask}},
            {{GESTURE_SOUTH_EAST}, {.fingers = 2, .reflectionMask = MirroredMask}},
        },
    },
    {
        {
            {{0, 0}, {1, 0}, {1, 1}},
            {{8, 8}, {7, 8}, {7, 9}},
        },
        {
            {{GESTURE_WEST, GESTURE_SOUTH}, {.fingers = 2, .reflectionMask = MirroredXMask}},
            {{GESTURE_EAST, GESTURE_SOUTH}, {.fingers = 2, .reflectionMask = MirroredXMask}},
        },
    },
    {
        {
            {{1, 1}, {1, 0}, {0, 0}},
            {{7, 9}, {7, 8}, {8, 8}},
        },
        {
            {{GESTURE_NORTH, GESTURE_EAST}, {.fingers = 2, .reflectionMask = MirroredXMask}},
            {{GESTURE_NORTH, GESTURE_WEST}, {.fingers = 2, .reflectionMask = MirroredXMask}},
        },
    },
    {
        {
            {{0, 0}, {0, 1}, {1, 1}},
            {{8, 8}, {8, 7}, {9, 7}},
        },
        {
            {{GESTURE_NORTH, GESTURE_EAST}, {.fingers = 2, .reflectionMask = MirroredYMask}},
            {{GESTURE_SOUTH, GESTURE_EAST}, {.fingers = 2, .reflectionMask = MirroredYMask}},
        },
    },
};
SCUTEST_ITER(generic_gesture_binding, LEN(genericGestureBinding)) {
    listenForGestureEvents(GestureEndMask);
    struct GenericGestureBindingCheck values = genericGestureBinding[_i];
    for(int i = 0; i < LEN(values.points); i++)
        startGestureWithPoints(values.points[i], LEN(values.points[i]), i);
    endGestureHelper(LEN(values.points));
    GestureEvent* event = getNextGesture();
    assert(event);
    assert(matchesGestureEvent(&values.bindings[0], event) || matchesGestureEvent(&values.bindings[1], event));
    assert(matchesGestureEvent(&values.bindings[0], event));
    event = getNextGesture();
    assert(matchesGestureEvent(&values.bindings[1], event));
}
static int count;
static void incrementCount() {
    count++;
}
static int getCount() {
    return count;
}

static volatile bool shutdown = 0;
void gestureEventLoop(GestureBinding* bindings, int N) {
    while(!shutdown) {
        GestureEvent* event = waitForNextGesture();
        if(event)
            triggerGestureBinding(bindings, N, event);
        free(event);
    }
}
static void requestShutdown() {
    shutdown = 1;
}
static void exitFailure() { exit(10);}

GestureEvent event = {.detail = {GESTURE_TAP}};
struct GestureBindingEventMatching {
    GestureEvent event;
    GestureBinding binding;
    int count;
} gestureBindingEventMatching [] = {
    {{.detail = {GESTURE_TAP}, .flags = {.count = 1, .fingers = 1, }}, {incrementCount, {{GESTURE_TAP}, {.count = 1}}}, 1},
    {{.detail = {GESTURE_TAP}, .flags = {.count = 2, .fingers = 1, }}, {incrementCount, {{GESTURE_TAP}, {.count = 1}}}, 0},
    {{.detail = {GESTURE_TAP}, .flags = {.count = 2, .fingers = 1, }}, {incrementCount, {{GESTURE_TAP}, {.count = 2}}}, 1},
    {{.detail = {GESTURE_TAP}, .flags = {.count = 1, .fingers = 1, }}, {incrementCount, {{GESTURE_UNKNOWN}, {.count = 1}}}, 0},
};
SCUTEST_ITER(gesture_matching, LEN(gestureBindingEventMatching)) {
    struct GestureBindingEventMatching values = gestureBindingEventMatching[_i];
    assert(matchesGestureEvent(&values.binding.arg, &values.event) == values.count);
    triggerGestureBinding(&values.binding, 1, &values.event);
    assert(getCount() == values.count);
}

SCUTEST(double_tap) {
    void _lambda() {
        incrementCount();
        requestShutdown();
    }
    GestureBinding bindings[] = {
        {_lambda, {{}, {.count = 2}}},
        {exitFailure, {{}, {.count = 1}}}
    };
    for(int i = 0; i < 2; i++) {
        startGestureTap(0);
        endGestureHelper(1);
    }
    gestureEventLoop(bindings, LEN(bindings));
    assert(getCount() == 1);
}

SCUTEST(differentiate_devices) {
    void _lambda() {
        if(getCount() == 2)requestShutdown();
    }
    GestureBinding bindings[] = {
        {incrementCount, {{}, {.count = 1}}},
        {exitFailure, {{}, {.count = 2}}},
        {_lambda, {{}, {.count = 1}}}
    };
    for(int i = 0; i < 2; i++) {
        startGestureWrapper(i, 0, (GesturePoint) {0, 0});
        endGestureWrapper(i, 0);
    }
    gestureEventLoop(bindings, LEN(bindings));
    assert(getCount() == 2);
}
