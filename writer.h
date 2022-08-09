#ifndef LIB_SGESUTRES_WRITER_H_
#define LIB_SGESUTRES_WRITER_H_

#include "touch.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

void setRawGestureEventNames(LargestRawGestureEvent* event, const char* sysname, const char* devname) {
    char* dest = strncpy(event->buffer, sysname, DEVICE_NAME_LEN);
    *dest = 0;
    dest = strncpy(dest + 1, devname, DEVICE_NAME_LEN);
    *dest = 0;
    event->event.totalNameLen = dest - event->buffer;
}

int writeTouchEvent(int fd, const RawGestureEvent* event) {
    return write(fd, event, sizeof(RawGestureEvent) + event->totalNameLen);
}
#endif
