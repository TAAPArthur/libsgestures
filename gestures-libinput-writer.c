/**
 * @file
 * Reads raw libinput touch events and converts them into our TouchEvent
 */
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <unistd.h>

#include "touch.h"
#include "writer.h"
#include "gestures-private.h"

/**
 * Opens a path with given flags. Path probably references an input device and likely starts with  /dev/input/
 * If user_data is not NULL and points to a non-zero value, then the device corresponding to path will be grabbed (@see EVIOCGRAB)
 *
 * @param path path to open
 * @param flags flags to open path with
 * @param user_data if true the device will be grabbed
 *
 * @return the file descriptor corresponding to the opened file
 */
static int open_restricted(const char* path, int flags, void* user_data) {
    bool* grab = (bool*)user_data;
    int fd = open(path, flags);
    if(fd >= 0 && grab && *grab && ioctl(fd, EVIOCGRAB, &grab) == -1) {
        char buffer[256] = "Grab requested, but failed for " ;
        strncat(buffer, path, 200);
        perror(buffer);
        exit(1);
    }
    return fd < 0 ? -errno : fd;
}
/**
 *
 * @param fd the file descriptor to close
 * @param user_data unused
 */
static void close_restricted(int fd, void* user_data __attribute__((unused))) {
    close(fd);
}

static struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};
static struct udev* udev = NULL;
struct libinput* createUdevInterface(bool grab) {
    udev = udev_new();
    struct libinput* li = libinput_udev_create_context(&interface, &grab, udev);
    libinput_udev_assign_seat(li, "seat0");
    return li;
}
struct libinput* createPathInterface(const char** paths, int num, bool grab) {
    struct libinput* li = libinput_path_create_context(&interface, &grab);
    for(int i = 0; i < num; i++)
        libinput_path_add_device(li, paths[i]);
    return li;
}
static inline double getX(struct libinput_event_touch* event) {
    return libinput_event_touch_get_x(event);
}
static inline double getY(struct libinput_event_touch* event) {
    return libinput_event_touch_get_y(event);
}

#define GESTURE_PIXEL_REF  100
static inline int getXInPixels(struct libinput_event_touch* event) {
    return libinput_event_touch_get_x_transformed(event, GESTURE_PIXEL_REF);
}
static inline int getYInPixels(struct libinput_event_touch* event) {
    return libinput_event_touch_get_y_transformed(event, GESTURE_PIXEL_REF);
}

void processTouchEvent(struct libinput_event_touch* event, enum libinput_event_type type) {
    GesturePoint point = {};
    GesturePoint pointPixel = {};
    int32_t seat;
    struct libinput_device* inputDevice;
    uint32_t id;
    uint32_t time;
    GestureMask mask = 0;
    switch(type) {
        default:
            break;
        case LIBINPUT_EVENT_TOUCH_MOTION:
            mask =  TouchMotionMask;
            break;
        case LIBINPUT_EVENT_TOUCH_DOWN:
            mask =  TouchStartMask;
            break;
        case LIBINPUT_EVENT_TOUCH_CANCEL:
            mask =  TouchCancelMask;
            break;
        case LIBINPUT_EVENT_TOUCH_UP:
            mask =  TouchEndMask;
            break;
    }
    switch(type) {
        default:
            break;
        case LIBINPUT_EVENT_TOUCH_MOTION:
        case LIBINPUT_EVENT_TOUCH_DOWN:
            point = (GesturePoint) { (int)getX(event), (int)getY(event)};
            pointPixel = (GesturePoint) {(int)getXInPixels(event), (int)getYInPixels(event)};
            [[fallthrough]];
        case LIBINPUT_EVENT_TOUCH_CANCEL:
        case LIBINPUT_EVENT_TOUCH_UP:
            inputDevice = libinput_event_get_device((struct libinput_event*)event);
            id = libinput_device_get_id_product(inputDevice);
            seat = libinput_event_touch_get_seat_slot(event);
            time = libinput_event_touch_get_time(event);
            write(1, &mask, sizeof(mask));
            TouchEvent touchEvent = {id, seat, point, pointPixel, time};
            write(1, &touchEvent, sizeof(TouchEvent));
            if(type == LIBINPUT_EVENT_TOUCH_DOWN) {
                const char* buffer = libinput_device_get_sysname(inputDevice);
                char size = strlen(buffer) + 1;
                write(1, &size, sizeof(size));
                write(1, buffer, size);
                buffer = libinput_device_get_name(inputDevice);
                size = strlen(buffer) + 1;
                write(1, &size, sizeof(size));
                write(1, buffer, size);
            }
            break;
    }
}
static volatile bool isListening = 0;
static int dummyFD;
void wakeupLibInputListener() {
    char num = 100;
    write(dummyFD, &num, sizeof(num));
}
void stopGestures() {
    isListening = 0;
    wakeupLibInputListener();
}

static void listenForGestures(struct libinput* li) {
    dummyFD = eventfd(0, EFD_CLOEXEC);
    struct libinput_event* event;
    uint32_t fd = libinput_get_fd(li);
    // POLLERR | POLLHUP are always implicitly selected
    struct pollfd fds[3] = {{fd, POLLIN | POLLHUP}, {STDOUT_FILENO, POLLERR | POLLHUP}, {(short)dummyFD, POLLIN}};
    isListening = 1;
    while(isListening) {
        poll(fds, LEN(fds), -1);
        if(fds[0].revents & (POLLERR | POLLHUP) || fds[1].revents & (POLLERR | POLLHUP)) {
            break;
        }
        if(fds[2].revents & POLLIN == 0) {
            char c;
            read(dummyFD, &c, sizeof(char));
            continue;
        }
        libinput_dispatch(li);
        while(event = libinput_get_event(li)) {
            enum libinput_event_type type = libinput_event_get_type(event);
            processTouchEvent((struct libinput_event_touch*)event, type);
            libinput_event_destroy(event);
            libinput_dispatch(li);
        }
    }
    libinput_unref(li);
    close(dummyFD);
}
/**
 * Starting listening for and processing gestures.
 * One thread is spawned to listen for libinput TouchEvents and convert them into our custom GestureEvents
 * Another thread processes these GestureEvents and triggers GestureBindings
 *
 * @param paths only listen for these paths
 * @param num length of paths array
 * @param grab rather to get an exclusive grab on the device
 */
void startGestures(const char** paths, int num, bool grab) {
    struct libinput* li = paths && num ? createPathInterface(paths, num, grab) : createUdevInterface(grab);
    listenForGestures(li);
}

int __attribute__((weak)) main(int argc, char* const argv[]) {
    bool grab = 0;
    int i = 1;
    if(argv[1] && strcmp(argv[1], "--grab")) {
        i++;
        grab = 1;
    }
    startGestures((const char**)argv, argc - i, grab);
}
