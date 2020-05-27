/**
 * @file
 *
 * Reads TouchEvents written by gestures-libpinput-writer
 */
#include "unistd.h"
#include <sys/poll.h>

#include "gestures-private.h"
#include "touch.h"

bool isTouchEventReady(int32_t fd) {
    struct pollfd fds[2] = {{fd, POLLIN}};
    poll(fds, 2, -1);
    return fds[0].revents & POLLIN != 0;
}
void readTouchEvent(uint32_t fd) {
    char bufferSysName[DEVICE_NAME_LEN];
    char bufferDeviceName[DEVICE_NAME_LEN];
    char size;
    GestureMask mask;
    TouchEvent touchEvent;
    read(fd, &mask, sizeof(mask));
    read(fd, &touchEvent, sizeof(touchEvent));
    switch(mask) {
        case TouchStartMask:
            read(fd, &size, sizeof(size));
            read(fd, bufferSysName, size);
            read(fd, &size, sizeof(size));
            read(fd, bufferDeviceName, size+1);
            startGesture(touchEvent,  bufferSysName, bufferDeviceName);
            break;
        case TouchMotionMask:
            continueGesture(touchEvent);
            break;
        case TouchEndMask:
            endGesture(touchEvent);
            break;
        case TouchCancelMask:
            cancelGesture(touchEvent);
        default:
            break;
    }
}
