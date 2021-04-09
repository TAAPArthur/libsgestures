/**
 * @file
 *
 * Reads TouchEvents written by gestures-libpinput-writer
 */
#include <unistd.h>
#include <poll.h>

#include "gestures-private.h"
#include "touch.h"

bool isTouchEventReady(int32_t fd) {
    struct pollfd event = {fd, POLLIN};
    return poll(&event, 2, -1) > 0 && event.revents & POLLIN;
}

#define safe_read(FD, VAR, SIZE)if(read(FD, VAR, SIZE)==0) return 0;
bool readTouchEvent(uint32_t fd) {
    char bufferSysName[DEVICE_NAME_LEN];
    char bufferDeviceName[DEVICE_NAME_LEN];
    char size;
    GestureMask mask;
    TouchEvent touchEvent;
    safe_read(fd, &mask, sizeof(mask));
    safe_read(fd, &touchEvent, sizeof(touchEvent));
    switch(mask) {
        case TouchStartMask:
            safe_read(fd, &size, sizeof(size));
            safe_read(fd, bufferSysName, size);
            safe_read(fd, &size, sizeof(size));
            safe_read(fd, bufferDeviceName, size);
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
    return 1;
}
