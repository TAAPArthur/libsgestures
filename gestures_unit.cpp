
#include "tester.h"
#include "gestures.h"
#include "gestures-helper.h"

#include <assert.h>
#include <condition_variable>
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <linux/input.h>
#include <list>
#include <math.h>
#include <mutex>
#include <stdbool.h>
#include <string.h>
#include <sys/poll.h>
#include <thread>
#include <unordered_map>
#include <vector>
#include <deque>

#define LEN(X) (sizeof X / sizeof X[0])

static int SCALE_FACTOR = 2 * THRESHOLD_SQ;
static void cleanup() {
    for(auto*b : getGestureBindings())
        delete b;
    getGestureBindings().clear();
}

MPX_TEST_ITER("gesture_types_string", GESTURE_UNKNOWN + 1, {
    getGestureTypeString((GestureType)_i);
});
MPX_TEST("gesture_detail_eq", {
    assertEquals(GESTURE_TAP, GESTURE_TAP);
    assertEquals(GestureDetail({GESTURE_TAP}), GestureDetail({GESTURE_TAP}));
    assertEquals(GestureDetail({GESTURE_TAP, GESTURE_TAP}), GestureDetail({GESTURE_TAP, GESTURE_TAP}));
    assert(!(GestureDetail({GESTURE_TAP}) == GestureDetail({GESTURE_NORTH_WEST})));
    assert(!(GestureDetail({GESTURE_TAP, GESTURE_TAP}) == GestureDetail({GESTURE_TAP})));
});
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
MPX_TEST_ITER("validate_dir", LEN(gestureTypeTuples), {
    auto values = gestureTypeTuples[_i];
    assertEquals(values.type, getOppositeDirection(values.typeOpposite));
    assertEquals(values.type, getOppositeDirection(getOppositeDirection(values.type)));
    assertEquals(values.type, getMirroredXDirection(values.typeMirrorX));
    assertEquals(values.type, getMirroredXDirection(getMirroredXDirection(values.type)));
    assertEquals(values.type, getMirroredYDirection(values.typeMirrorY));
    assertEquals(values.type, getMirroredYDirection(getMirroredYDirection(values.type)));
    assertEquals(values.type, getRot270Direction(values.rot90));
    assertEquals(values.type, getRot90Direction(getRot90Direction(values.typeOpposite)));
    assertEquals(values.type, getRot90Direction(getRot270Direction(values.type)));
    assertEquals(values.type, getRot270Direction(getRot270Direction(values.typeOpposite)));
});
static int FAKE_DEVICE_ID = 0;
static unsigned int timeCounter = 0;
static void continueGesture(ProductID id, int32_t seat, GesturePoint point) {
    continueGesture({id, seat, point, point, timeCounter++});
}
static void startGesture(ProductID id, int32_t seat, GesturePoint point) {
    startGesture({id, seat, point, point, timeCounter++});
}
static void endGesture(ProductID id, int32_t seat){
    endGesture({id, seat, {}, {}, timeCounter++});
}
void cancelGesture(ProductID id, int32_t seat){
    cancelGesture({id, seat, {}, {}, timeCounter++});
}
GesturePoint operator *(const GesturePoint p, int n) {
    return {p.x * n, p.y * n};
}
GesturePoint operator +(const GesturePoint& p1, const GesturePoint p2){
    return {p1.x + p2.x, p1.y + p2.y};
}
GesturePoint operator -(const GesturePoint& p1, const GesturePoint p2){
    return {p1.x - p2.x, p1.y - p2.y};
}
static void startGestureHelper(const std::vector<GesturePoint>& points, int seat = 0, int steps = 0, GesturePoint offset = {0, 0}) {
    assert(points.size());
    startGesture(FAKE_DEVICE_ID, seat, points[0]*SCALE_FACTOR + offset);
    for(int i = 1; i < points.size(); i++) {
        if(steps) {
            auto delta = (points[i] - points[i - 1]);
            delta /= steps;
            GesturePoint p = points[i - 1];
            for(int s = 0; s < steps - 1; s++) {
                p += delta;
                continueGesture(FAKE_DEVICE_ID, seat, p * SCALE_FACTOR + offset);
            }
        }
        continueGesture(FAKE_DEVICE_ID, seat, points[i]*SCALE_FACTOR + offset);
    }
}
static void endGestureHelper(int fingers = 1) {
    for(int n = 1; n < fingers; n++)
        endGesture(FAKE_DEVICE_ID, n);
    endGesture(FAKE_DEVICE_ID, 0);
}

static void listenForAllGestures() {
    listenForGestureEvents(-1);
}
SET_ENV(listenForAllGestures, cleanup);
MPX_TEST("start_continue_end_gesture", {
    startGestureHelper({{0,0}, {0,0}}, 0);
    endGestureHelper(1);
    auto* event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchStartMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchHoldMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchEndMask);
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == GestureEndMask);
    assert(!getNextGesture());
})
MPX_TEST_ITER("start_continue_end_gesture_tap", 3,{
    int n = _i +1;
    for(int i = 0;i < n; i++)
        startGestureHelper({{0,0}, {0,0}}, i);
    endGestureHelper(n);
    for(int i = 0; i< 3 * n + 1; i ++) {
        auto* event = getNextGesture();
        assert(event);
        assert(event->detail[0] == GESTURE_TAP);
    }
})

MPX_TEST("reuse_seats", {
    listenForGestureEvents(GestureEndMask);
    startGestureHelper({{0,0}}, 0);
    int n = 10;
    for(int i = 1; i < n; i++){
        startGestureHelper({{0,0}}, 1);
        endGesture(FAKE_DEVICE_ID, 1);
    }
    endGesture(FAKE_DEVICE_ID, 0);
    auto* event = getNextGesture();
    assert(event);
    assert(event->flags.fingers == n);
})

MPX_TEST("reset_fingers", {
    for(int i = 0;i < 10; i++){
        startGestureHelper({{0,0}, {0,0}}, 0);
        endGestureHelper(1);
        for(int i = 0; i< 3; i ++) {
            auto* event = getNextGesture();
            assert(event);
            assert(event->flags.fingers == 1);
        }
    }
})

MPX_TEST("cancel_reset",{
    listenForGestureEvents(TouchCancelMask | GestureEndMask);
    startGestureHelper({{0,0}}, 0);
    cancelGesture(FAKE_DEVICE_ID, 0);
    startGestureHelper({{0,0}}, 1);
    endGesture(FAKE_DEVICE_ID, 1);
    auto* event = getNextGesture();
    assert(event);
    assert(event->flags.mask == TouchCancelMask );
    event = getNextGesture();
    assert(event);
    assert(event->flags.mask == GestureEndMask);
    assert(event->flags.fingers == 1);
});

GesturePoint lineDelta[] = {
    [0] = {},
    [1] = {},
    [2] = {},
    [3] = {},
    [GESTURE_NORTH_WEST] = {-1, -1},
    [GESTURE_WEST] = {-1, 0},
    [GESTURE_SOUTH_WEST] = {-1, 1},
    [7] = {},
    [GESTURE_NORTH] = {0, -1},
    [GESTURE_TAP] = {0, 0},
    [GESTURE_SOUTH] = {0, 1},
    [11] = {},
    [GESTURE_NORTH_EAST] = {1, -1},
    [GESTURE_EAST] = {1, 0},
    [GESTURE_SOUTH_EAST] = {1, 1},
};

struct GestureEventChecker {
    GestureDetail detail;
    std::vector<GesturePoint> points;
} gestureEventTuples[] = {
    {{GESTURE_NORTH_WEST, GESTURE_SOUTH_WEST}, {{2, 2}, {1, 1}, {0, 2}}},
    {{GESTURE_WEST}, {{1, 1}, {0, 1}}},
    {{GESTURE_SOUTH_WEST, GESTURE_SOUTH_EAST}, {{1, 1}, {0, 2}, {1, 3}}},
    {{GESTURE_NORTH}, {{1, 1}, {1, 0}}},
    {{GESTURE_TAP}, {{1, 1}}},
    {{GESTURE_TAP}, {{1, 1}, {1, 1}}},
    {{GESTURE_SOUTH}, {{1, 1}, {1, 2}}},
    {{GESTURE_NORTH_EAST, GESTURE_NORTH_WEST}, {{2, 2}, {3, 1}, {2, 0}}},
    {{GESTURE_EAST}, {{1, 1}, {2, 1}}},
    {{GESTURE_SOUTH_EAST, GESTURE_NORTH_EAST}, {{1, 1}, {2, 2}, {3, 1}}},
    {{GESTURE_EAST}, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}},
};

static void listenForGestureEnd() {
    listenForGestureEvents(GestureEndMask);
}

SET_ENV(listenForGestureEnd, cleanup);
MPX_TEST_ITER("validate_line_dir", LEN(gestureEventTuples) * 4, {
    int i = _i / 4;
    int fingers = _i % 2 + 1;
    bool cancel = _i % 4 > 2;
    auto& values = gestureEventTuples[i];
    assert(fingers);
    if(cancel) {
        startGestureHelper(values.points, -1);
        cancelGesture(FAKE_DEVICE_ID, -1);
    }
    for(int n = 0; n < fingers; n++)
        startGestureHelper(values.points, n);
    endGestureHelper(fingers);
    auto event = getNextGesture();
    assert(event);
    assert(event->detail.size());
    assertEquals(event->detail, values.detail);
    assertEquals(event->flags.fingers, fingers);
    assert(!event->flags.reflectionMask);
});
GestureType getLineType(GesturePoint start, GesturePoint end);
static const std::vector<GesturePoint> getGesturePointsThatMakeLine(const GestureDetail detail) {
    assert(detail.size());
    GesturePoint p = {8, 8};
    std::vector<GesturePoint>points = {p};
    for(int i = 0; i < detail.size(); i++){
        auto type = detail[i];
        points.push_back(p += lineDelta[type]);
        if(type != GESTURE_TAP)
            assertEquals(getLineType(points[points.size() - 2], p), type);
    }
    return points;
}
MPX_TEST_ITER("validate_masks", LEN(gestureEventTuples)*LEN(MASKS), {
    int index = _i / LEN(MASKS);
    auto mask = MASKS[_i % LEN(MASKS)];
    auto values = gestureEventTuples[index];
    GestureBinding bindingBase = GestureBinding({values.detail}, {}, {});
    GestureBinding bindingRefl = GestureBinding({values.detail}, {}, {.reflectionMask = mask});
    startGestureHelper(getGesturePointsThatMakeLine(bindingBase.getDetails()));
    startGestureHelper(getGesturePointsThatMakeLine(bindingBase.getDetails()), 1, 0, {500, 500});
    endGestureHelper(2);
    auto event = getNextGesture();
    assert(!event->flags.reflectionMask || event->flags.reflectionMask & mask);
});

struct GestureMultiLineCheck {
    std::vector<GesturePoint>points;
    GestureDetail detail;
    int fingers = 1;
    int steps = 0;
} multiLines[] = {
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
    },
    {
        {{1, 1}, {2, 2}, {3, 1}, {2, 0}, {1, 1}},
        {GESTURE_SOUTH_EAST, GESTURE_NORTH_EAST, GESTURE_NORTH_WEST, GESTURE_SOUTH_WEST},
    },
    {
        {{0, 0}, {1, 0}, {1, 1}},
        {GESTURE_EAST, GESTURE_SOUTH},
        .fingers = 2
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        .fingers = 2
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        .steps = 10
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}},
        {GESTURE_EAST, GESTURE_SOUTH, GESTURE_WEST, GESTURE_NORTH},
        10, 10
    },
    {
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 3}, {3, 9}},
        {GESTURE_EAST, GESTURE_SOUTH},
        10, 10
    },
};
MPX_TEST_ITER("multi_lines", LEN(multiLines) * 2, {
    auto& values = multiLines[_i / 2];
    int step = (_i % 2) * 10;
    for(int i = 0; i < values.fingers; i++)
        startGestureHelper(values.points, i, step);
    endGestureHelper(values.fingers);
    auto event = getNextGesture();
    assert(!event->flags.reflectionMask);
    assertEquals(event->detail, values.detail);
});
struct GenericGestureCheck {
    std::vector<std::vector<GesturePoint>>points;
    GestureDetail detail;

    int getFingers() {return points.size();}
    int steps = 0;
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
            {{1, 1}},
            {{0, 1}, {0, 2}, {2, 2}},
        },
        {GESTURE_UNKNOWN},
    },
};
MPX_TEST_ITER("generic_gesture", LEN(genericGesture), {
    auto& values = genericGesture[_i];
    for(int i = 0; i < values.getFingers(); i++)
        startGestureHelper(values.points[i], i);
    endGestureHelper(values.getFingers());
    auto event = getNextGesture();
    assert(!event->flags.reflectionMask);
    assertEquals(event->detail, values.detail);
});

struct GenericGestureBindingCheck {
    std::vector<std::vector<GesturePoint>>points;
    GestureBinding bindings[2];
    int getFingers() {return points.size();}
    int steps = 0;
} genericGestureBinding[] = {
    {
        {
            {{1, 1}, {2, 2}},
            {{2, 2}, {1, 1}},
        },
        {
            {{GESTURE_NORTH_WEST}, {}, {.fingers = 2, .reflectionMask = MirroredMask}},
            {{GESTURE_SOUTH_EAST}, {}, {.fingers = 2, .reflectionMask = MirroredMask}},
        },
    },
    {
        {
            {{0, 0}, {1, 0}, {1, 1}},
            {{8, 8}, {7, 8}, {7, 9}},
        },
        {
            {{GESTURE_WEST, GESTURE_SOUTH}, {}, {.fingers = 2, .reflectionMask = MirroredXMask}},
            {{GESTURE_EAST, GESTURE_SOUTH}, {}, {.fingers = 2, .reflectionMask = MirroredXMask}},
        },
    },
    {
        {
            {{1, 1}, {1, 0}, {0, 0}},
            {{7, 9}, {7, 8}, {8, 8}},
        },
        {
            {{GESTURE_NORTH, GESTURE_EAST}, {}, {.fingers = 2, .reflectionMask = MirroredXMask}},
            {{GESTURE_NORTH, GESTURE_WEST}, {}, {.fingers = 2, .reflectionMask = MirroredXMask}},
        },
    },
    {
        {
            {{0, 0}, {0, 1}, {1, 1}},
            {{8, 8}, {8, 7}, {9, 7}},
        },
        {
            {{GESTURE_NORTH, GESTURE_EAST}, {}, {.fingers = 2, .reflectionMask = MirroredYMask}},
            {{GESTURE_SOUTH, GESTURE_EAST}, {}, {.fingers = 2, .reflectionMask = MirroredYMask}},
        },
    },
};
MPX_TEST_ITER("generic_gesture_binding", LEN(genericGestureBinding), {
    listenForGestureEvents(GestureEndMask);
    auto& values = genericGestureBinding[_i];
    for(int i = 0; i < values.getFingers(); i++)
        startGestureHelper(values.points[i], i);
     endGestureHelper(values.getFingers());
    auto* event = getNextGesture();
    assert(event);
    assert(values.bindings[0].matches(*event)||values.bindings[1].matches(*event));
    assert(values.bindings[0].matches(*event));
    event = getNextGesture();
    assert(values.bindings[1].matches(*event));
});
static int count;
static void incrementCount() {
    count++;
}
static int getCount() {
    return count;
}

static volatile bool shutdown = 0;
void gestureEventLoop() {
    while(!shutdown) {
        auto*event=waitForNextGesture();
        if(event)
            triggerGestureBinding(*event);
        delete event;
    }
}
static void requestShutdown(){
    shutdown = 1;
}
static void exitFailure(){ exit(10);}

GestureEvent event = {.detail = {GESTURE_TAP}};
struct GestureBindingEventMatching {
    GestureEvent event;
    GestureBinding binding;
    int count = 1;
} gestureBindingEventMatching [] {
    {{.detail = {GESTURE_TAP}}, {{GESTURE_TAP}, incrementCount}},
    {{.detail = {GESTURE_TAP}, .flags = {.count = 2}}, {{GESTURE_TAP}, incrementCount, {.count = 1}}, 0},
    {{.detail = {GESTURE_TAP}, .flags = {.count = 2}}, {{GESTURE_TAP}, incrementCount, {.count = 2}}},
    {{.detail = {GESTURE_TAP}}, {{GESTURE_UNKNOWN}, incrementCount}, 0},
};
MPX_TEST_ITER("gesture_matching", LEN(gestureBindingEventMatching), {
    auto& values = gestureBindingEventMatching[_i];
    getGestureBindings().push_back(&values.binding);
    triggerGestureBinding(values.event);
    assertEquals(getCount(), values.count);
    getGestureBindings().clear();
});

MPX_TEST("double_tap", {
    getGestureBindings().push_back(new GestureBinding({}, []{incrementCount(); requestShutdown();}, {.count = 2}));
    getGestureBindings().push_back(new GestureBinding({}, exitFailure, {.count = 1}));
    for(int i = 0; i < 2; i++) {
        startGestureHelper({{0, 0}}, 0);
        endGestureHelper(1);
    }
    gestureEventLoop();
    assertEquals(getCount(), 1);
});

MPX_TEST("differentiate_devices", {
    getGestureBindings().push_back(new GestureBinding({}, incrementCount, {.count = 1}));
    getGestureBindings().push_back(new GestureBinding({}, exitFailure, {.count = 2}));
    getGestureBindings().push_back(new GestureBinding({}, []{if(getCount() == 2)requestShutdown();}, {.count = 1}));

    for(int i = 0; i < 2; i++) {
        startGesture(i, 0, {0, 0});
        endGesture(i, 0);
    }

    gestureEventLoop();
    assert(getCount() == 2);
});

/*
MPX_TEST("bad-path", {
    const char* path = "/dev/null";
    startGestures(&path, 1, 1);
    stopGestures();
});
MPX_TEST("udev", {
    startGestures();
    stopGestures();
});
*/
