#ifndef LIB_SGESUTRES_HELPER_H_
#define LIB_SGESUTRES_HELPER_H_
#include "gestures.h"
#include <string>

struct TouchEvent {
    uint32_t id;
    int32_t seat;
    GesturePoint point;
    GesturePoint pointPixel;
    uint32_t time;
};


/**
 * Starts a gesture
 *
 * @param id device id
 * @param seat seat number
 * @param startingGesturePoint
 * @param lastGesturePoint
 * @param sysName
 * @param name
 */
void startGesture(TouchEvent event, std::string sysName = "", std::string name = "");
/**
 * Adds a point to a gesture
 *
 * @param id
 * @param seat
 * @param point
 * @param lastGesturePoint
 */
void continueGesture(TouchEvent event);
/**
 * Concludes a gestures and if all gestures in the GestureGroup are done, termines the group and returns a GestureEvent.
 * The event is already added to the queue for processing
 *
 * @param id
 * @param seat
 *
 * @return NULL of a gesture event if all gestures have completed
 */
void endGesture(TouchEvent event);
/**
 * Aborts and ongoing gesture
 *
 * Does not generate any events. Already generated events are not affected
 *
 * @param id
 * @param seat
 */
void cancelGesture(TouchEvent event);

/**
 * Starting listening for and processing gestures.
 * One thread is spawned to listen for libinput TouchEvents and convert them into our custom GestureEvents
 * Another thread processes these GestureEvents and triggers GestureBindings
 *
 * @param paths only listen for these paths
 * @param num length of paths array
 * @param grab rather to get an exclusive grab on the device
 */
void startGestures(const char** paths = NULL, int num = 0, bool grab = 0);
#endif
