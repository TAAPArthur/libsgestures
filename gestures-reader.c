/**
 * @file
 *
 * Reads TouchEvents written by gestures-libpinput-writer
 */
#include <unistd.h>
#include <poll.h>
#include <string.h>

#include "gestures-private.h"
#include "touch.h"

bool isTouchEventReady(int32_t fd) {
    struct pollfd event = {fd, POLLIN};
    return poll(&event, 2, -1) > 0 && event.revents & POLLIN;
}

#define safe_read(FD, VAR, SIZE)do {int ret = read(FD, VAR, SIZE); if(ret <= 0) return ret;} while(0)
bool readTouchEvent(uint32_t fd) {
    char buffer[DEVICE_NAME_LEN * 2];
    char size;
    RawGestureEvent event;
    safe_read(fd, &event, sizeof(event));
    switch(event.mask) {
        case TouchStartMask:
            safe_read(fd, buffer, event.totalNameLen);
            startGesture(event.touchEvent,  buffer, buffer + strnlen(buffer, DEVICE_NAME_LEN));
            break;
        case TouchMotionMask:
            continueGesture(event.touchEvent);
            break;
        case TouchEndMask:
            endGesture(event.touchEvent);
            break;
        case TouchCancelMask:
            cancelGesture(event.touchEvent);
        default:
            return -1;
    }
    return 1;
}
