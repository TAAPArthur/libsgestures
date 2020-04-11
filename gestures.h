/**
 * @file
 */
#ifndef LIB_SGESUTRES_H_
#define LIB_SGESUTRES_H_
#include "assert.h"
#include "string.h"
#include <string>
#include <functional>
#include <vector>

#define MAX_GESTURE_DETAIL_SIZE 128

/// The consecutive points within this distance are considered the same and not double counted
#define THRESHOLD_SQ (100)
/// All seat of a gesture have to start/end within this sq distance of each other
#define PINCH_THRESHOLD (256)
/// The cutoff for when a sequence of points forms a line
#define R_SQUARED_THRESHOLD .5

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


/// ID of touch device
typedef uint32_t ProductID;
/// Groups gestures together; based on ProductID
typedef uint64_t GestureGroupID;
/// Id of a particular touch device based on ProductID and seat number
typedef uint64_t TouchID;


/**
 * Helper class for Rect
 */
struct GesturePoint {
    /// x coordinate
    int32_t x;
    /// y coordinate
    int32_t y;
    /**
     * Adds p's coordinates with to this structs
     * @param p
     * @return *this
     */
    GesturePoint& operator +=(const GesturePoint& p) {
        x += p.x;
        y += p.y;
        return *this;
    }
    /**
     * Scales the coordinates by 1/n
     * @param n
     * @return *this
     */
    GesturePoint& operator /=(int n) {
        x /= n;
        y /= n;
        return *this;
    }
};

/**
 * Controls what type(s) of gestures match a binding.
 * The Mirrored*Masks allow for the specified gesture or a reflection to match
 */
enum TransformMasks {
    TransformNone = 0,
    /// Modifier used to indicate that a gesture was reflected across the X axis
    MirroredXMask = 1,
    /// Modifier used to indicate that a gesture was reflected across the Y axis
    MirroredYMask = 2,
    /// Modifier used to indicate that a gesture was reflected across the X and Y axis
    MirroredMask = 3,
    Rotate90Mask = 4,
    Rotate270Mask = 8,

};
/**
 * Only Gesture with mask contained by mask will able to trigger events
 * By default all gestures are considered
 * @param mask
 */
void listenForGestureEvents(uint32_t mask);
/**
 * Types of gestures
 */
enum GestureType {

    GESTURE_NONE = 0,

    /// A gesture not defined by any of the above
    GESTURE_UNKNOWN = 1,

    /// Represents fingers coming together
    GESTURE_PINCH = 2,
    /// Represents fingers coming apart
    GESTURE_PINCH_OUT = 3,
    /// @{ Gesture in a straight line
    GESTURE_NORTH_WEST = 4,
    GESTURE_WEST,
    GESTURE_SOUTH_WEST,
    GESTURE_NORTH = 8,
    GESTURE_TAP,
    GESTURE_SOUTH,
    GESTURE_NORTH_EAST = 12,
    GESTURE_EAST,
    GESTURE_SOUTH_EAST,
    /// @}


    /// Too many gestures
    GESTURE_TOO_LARGE = 15,
};

/**
 * Returns the opposite direction of d
 * For example North -> South, SouthWest -> NorthEast
 * @param d
 * @return the opposite direction of d
 */
GestureType getOppositeDirection(GestureType d);
/**
 * Returns d flipped across the X axis
 * For exmaple North -> North, East -> West, SouthWest -> SouthEast
 * @param d
 * @return d reflected across the x axis
 */
GestureType getMirroredXDirection(GestureType d);
/**
 * Returns d flipped across the Y axis
 * For exmaple North -> South, East -> East, SouthWest -> NorthWest
 * @param d
 * @return d reflected across the x asis
 */
GestureType getMirroredYDirection(GestureType d);

/**
 * Returns d roated 90deg counter clockwise
 * For exmaple North -> West, West-> South, SouthWest -> SouthEast
 * @param d
 * @return d roated 90 deg
 */
GestureType getRot90Direction(GestureType d);

/**
 * Returns d roated 270deg counter clockwise
 * For exmaple North -> East, West-> North, SouthWest -> NorthWest
 * @param d
 * @return d roated 270 deg
 */
GestureType getRot270Direction(GestureType d);
/**
 * Transforms type according to mask
 *
 * @param mask
 * @param type
 *
 * @return
 */
static inline GestureType getReflection(TransformMasks mask, GestureType type) {
    return mask == MirroredXMask ? getMirroredXDirection(type)
        : mask == MirroredYMask ? getMirroredYDirection(type)
        : mask == MirroredMask ? getOppositeDirection(type)
        : mask == Rotate90Mask ? getRot90Direction(type)
        : mask == Rotate270Mask ? getRot270Direction(type)
        : GESTURE_UNKNOWN;
}

/**
 * Simple struct used to check if a value is in range
 */
struct MinMax {
    /// min value
    uint32_t min;
    /// max value
    uint32_t max;
    /**
     * Contains exactly value
     *
     * @param value the new value of both min and max
     */
    MinMax(uint32_t value): min(value), max(value) {}
    /**
     * @param minValue
     * @param maxValue
     */
    MinMax(uint32_t minValue, uint32_t maxValue): min(minValue), max(maxValue) {}
    /**
     * @param value
     * @return 1 iff value is in [min, max]
     */
    bool contains(uint32_t value) const {
        return max == 0 || min <= value && value <= max;
    }
    /// @return max
    operator uint32_t () const { return max;}
    /**
     * @param value adds value to max
     * @return
     */
    MinMax& operator +=(uint32_t value) {
        max += value;
        return *this;
    }
    /**
     * @param value the amount to divide max by
     *
     * @return
     */
    MinMax& operator /=(uint32_t value) {
        max /= value;
        return *this;
    }
};


/// special flags specific to gestures
struct GestureFlags {
    /// The number of occurrences of this gesture
    /// The subsequent, time-sensitive repeat count of this event
    /// Identical events within GESTURE_MERGE_DELAY_TIME are combined and this count is incremented
    /// Only applies to events with the same id
    MinMax count = 1;
    /// The total distance traveled
    MinMax totalSqDistance = 0;
    /// The total displacement traveled averaged across all fingers
    MinMax avgSqDisplacement = 0;
    /// The total distance traveled averaged across all fingers
    MinMax avgSqDistance = 0;
    /// The time in ms since the first to last press for this gesture
    MinMax duration = 0;
    /// The number of fingers used in this gesture
    MinMax fingers = 1;
    /// @copydoc TransformMasks
    TransformMasks reflectionMask;
    /// If reflectionMask, instead of appending the transformation, it will replace this base (for GestureBindings)
    bool replaceWithTransform = 0;

    /// GestureMask; matches events with a mask contained by this value
    uint32_t mask = GestureEndMask;
    /**
     * @param flags from an event
     * @return 1 iff this GestureBinding's flags are compatible with flags
     */
    bool matchesFlags(const GestureFlags& flags) const;
    /// @return @copydoc mask
    uint32_t getMask() const { return mask;}
};


struct GestureDetail{
    GestureType detail[MAX_GESTURE_DETAIL_SIZE];
    void push_back(GestureType type){ assert(size() != MAX_GESTURE_DETAIL_SIZE); detail[size()]=type;}
    int size()const {
        for(int i = 0; i<MAX_GESTURE_DETAIL_SIZE;i++)
            if(detail[i] == GESTURE_NONE)
                return i;
        return MAX_GESTURE_DETAIL_SIZE;
    }

    GestureType operator [](int i) const { return detail[i];}
    bool operator ==(const GestureDetail& other) const {
        return memcmp(detail, other.detail, sizeof(detail)) == 0;
    }
};

/**
 * Gesture specific UserEvent
 */
struct GestureEvent {

    /// monotonically increasing number
    static uint32_t seqCounter;
    /// identifier use to potentially combine identical gestures
    const uint32_t seq = seqCounter++;
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

    /// @return the gesture region id or 0
    uint32_t getRegionID() const { return id >> 32; }
    /**
     * @return The libinput device id
     */
    uint32_t getDeviceID() const { return (ProductID)id ; }
    const GestureDetail getDetail() const { return detail; }

    /**
     * @param event
     * @return 1 iff this event was created before event
     */
    bool operator<(GestureEvent& event) {
        return seq < event.seq;
    }
    /**
     * @param event
     * @return
     */
    bool operator==(const GestureEvent& event) {
        return id == event.id && detail == event.getDetail() && flags.fingers == event.flags.fingers &&
            flags.mask == event.flags.mask;
    }
    /// Sleeps until GESTURE_MERGE_DELAY_TIME ms have passed since creation
    void wait() const;
};

/**
 * Generates a list of gesture keybindings based on flags
 *
 * @param keyBinding
 * @param flags
 *
 * @return
 */
GestureDetail transformGestureDetail(const GestureDetail& keyBinding, const TransformMasks mask);
/**
 * @param t
 * @return string representation of t
 */
const char* getGestureTypeString(GestureType t);

/**
 * @return the number of queued events
 */
int getGestureEventQueueSize();
/**
 * Gesture specific bindings
 */
struct GestureBinding {
    /// special flags specific to gestures
    const GestureDetail detail;
    const std::function<void()> func;
    const GestureFlags gestureFlags;
    const std::string name = "";
    ProductID regionID = 0;
    ProductID deviceID = 0;

    GestureBinding(GestureDetail _detail, const std::function<void()> _funcPtr , const GestureFlags _gestureFlags = {},
        std::string name = ""): detail(_detail) , func(_funcPtr),
                gestureFlags(_gestureFlags), name(name) {
    }
    virtual ~GestureBinding() = default;

    virtual bool matches(const GestureEvent& event) const;
    const GestureDetail& getDetails() const{return detail;};
};

/**
 * @return A list of GestureBindings
 */
std::vector<GestureBinding*>& getGestureBindings();

void triggerGestureBinding(GestureEvent&event);
GestureEvent* waitForNextGesture();
GestureEvent* getNextGesture();
bool isNextGestureReady();
uint32_t getGestureQueueSize();

void readTouchEvent(uint32_t fd);

ProductID __attribute__((weak)) generateIDHighBits(ProductID id, GesturePoint startingGesturePoint );
#endif
