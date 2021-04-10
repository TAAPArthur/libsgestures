/**
 * @file
 */
#ifndef LIB_SGESUTRES_H_
#define LIB_SGESUTRES_H_
#include <assert.h>
#include <string.h>
#include "touch.h"

#define MAX_GESTURE_DETAIL_SIZE 128


/// ID of touch device
typedef uint32_t ProductID;
/// Groups gestures together; based on ProductID
typedef uint64_t GestureGroupID;
/// Id of a particular touch device based on ProductID and seat number
typedef uint64_t TouchID;


/**
 * Controls what type(s) of gestures match a binding.
 * The Mirrored*Masks allow for the specified gesture or a reflection to match
 */
typedef enum {
    TransformNone = 0,
    /// Modifier used to indicate that a gesture was reflected across the X axis
    MirroredXMask = 1,
    /// Modifier used to indicate that a gesture was reflected across the Y axis
    MirroredYMask = 2,
    /// Modifier used to indicate that a gesture was reflected across the X and Y axis
    MirroredMask = 3,
    Rotate90Mask = 4,
    Rotate270Mask = 8,

} TransformMasks ;
/**
 * Only Gesture with mask contained by mask will able to trigger events
 * By default all gestures are considered
 * @param mask
 */
void listenForGestureEvents(uint32_t mask);

/**
 * Types of gestures
 */
typedef enum {
    GESTURE_NONE = 0,

    /// A gesture not defined by any of the above
    GESTURE_UNKNOWN = 1,

    /// Represents fingers coming together
    GESTURE_PINCH = 2,
    /// Represents fingers coming apart
    GESTURE_PINCH_OUT = 3,
    GESTURE_TAP = 4,
    /// Too many gestures
    GESTURE_TOO_LARGE = 5,
    GESTURE_EAST            = 0b1000,
    GESTURE_NORTH_EAST      = 0b1001,
    GESTURE_NORTH           = 0b1010,
    GESTURE_NORTH_WEST      = 0b1011,
    GESTURE_WEST            = 0b1100,
    GESTURE_SOUTH_WEST      = 0b1101,
    GESTURE_SOUTH           = 0b1110,
    GESTURE_SOUTH_EAST      = 0b1111,

} GestureType;

/**
 * Returns the opposite direction of d
 * For example North -> South, SouthWest -> NorthEast
 * @param d
 * @return the opposite direction of d
 */
GestureType getOppositeDirection(GestureType d);
/**
 * Returns d flipped across the X axis
 * For example North -> North, East -> West, SouthWest -> SouthEast
 * @param d
 * @return d reflected across the x axis
 */
GestureType getMirroredXDirection(GestureType d);
/**
 * Returns d flipped across the Y axis
 * For example North -> South, East -> East, SouthWest -> NorthWest
 * @param d
 * @return d reflected across the x axis
 */
GestureType getMirroredYDirection(GestureType d);

/**
 * Returns d rotated 90deg counter clockwise
 * For example North -> West, West-> South, SouthWest -> SouthEast
 * @param d
 * @return d rotated 90 deg
 */
GestureType getRot90Direction(GestureType d);

/**
 * Returns d rotated 270deg counter clockwise
 * For example North -> East, West-> North, SouthWest -> NorthWest
 * @param d
 * @return d rotated 270 deg
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

/// special flags specific to gestures
typedef struct GestureFlags {
    /// The total distance traveled
    uint32_t totalSqDistance;
    /// The total displacement traveled averaged across all fingers
    uint32_t avgSqDisplacement;
    /// The total distance traveled averaged across all fingers
    uint32_t avgSqDistance;
    /// The time in ms since the first to last press for this gesture
    uint32_t duration;
    /// The number of fingers used in this gesture
    uint32_t fingers;
    /// @copydoc TransformMasks
    TransformMasks reflectionMask;
    /// If reflectionMask, instead of appending the transformation, it will replace this base (for GestureBindings)
    bool replaceWithTransform;

    /// GestureMask; matches events with a mask contained by this value
    /// TODO
    GestureMask mask ;
} GestureFlags ;

typedef GestureType GestureDetail[MAX_GESTURE_DETAIL_SIZE];

GestureType getGestureType(const GestureDetail detail, int N);

int getNumOfTypes(const GestureDetail detail);
bool areDetailsEqual(const GestureDetail detail, const GestureDetail detail2);

/**
 * Generates a list of gesture keybindings based on flags
 *
 * @param keyBinding
 * @param flags
 *
 * @return
 */
GestureType* transformGestureDetail(GestureDetail detail, const TransformMasks mask);
/**
 * @param t
 * @return string representation of t
 */
const char* getGestureTypeString(GestureType t);

const char* getGestureMaskString(GestureMask mask);

ProductID __attribute__((weak)) generateIDHighBits(const TouchEvent* event);
#endif
