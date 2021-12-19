#ifndef LIB_SGESUTRES_WRITER_H_
#define LIB_SGESUTRES_WRITER_H_

#include <stdbool.h>
/**
 * Starting listening for and processing gestures.
 * One thread is spawned to listen for libinput TouchEvents and convert them into our custom GestureEvents
 * Another thread processes these GestureEvents and triggers GestureBindings
 *
 * @param paths only listen for these paths
 * @param num length of paths array
 * @param grab rather to get an exclusive grab on the device
 */
void startGestures(const char** paths, int num, bool grab);
void stopGestures();
#endif
