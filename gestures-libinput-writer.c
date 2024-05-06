/**
 * @file
 * Reads raw libinput touch events and converts them into our TouchEvent
 */
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "gestures-private.h"
#include "touch.h"
#include "writer.h"

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
    bool grab = (bool)user_data;
    int fd = open(path, flags);
    if(fd >= 0 && grab && ioctl(fd, EVIOCGRAB, &grab) == -1) {
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
    struct libinput* li = libinput_udev_create_context(&interface, (void*)(long)grab, udev);
    if (li) {
        libinput_udev_assign_seat(li, "seat0");
    }
    return li;
}
struct libinput* createPathInterface(const char** paths, int num, bool grab) {
    struct libinput* li = libinput_path_create_context(&interface, (void*)(long)grab);
    if (li) {
        for(int i = 0; i < num; i++)
            libinput_path_add_device(li, paths[i]);
    }
    return li;
}
static inline double getX(struct libinput_event_touch* event) {
    return libinput_event_touch_get_x(event);
}
static inline double getY(struct libinput_event_touch* event) {
    return libinput_event_touch_get_y(event);
}

static inline int getXInPercent(struct libinput_event_touch* event) {
    return libinput_event_touch_get_x_transformed(event, 100);
}
static inline int getYInPercent(struct libinput_event_touch* event) {
    return libinput_event_touch_get_y_transformed(event, 100);
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
            pointPixel = (GesturePoint) {(int)getXInPercent(event), (int)getYInPercent(event)};
             __attribute__ ((fallthrough));
        case LIBINPUT_EVENT_TOUCH_CANCEL:
        case LIBINPUT_EVENT_TOUCH_UP:
            inputDevice = libinput_event_get_device((struct libinput_event*)event);
            id = libinput_device_get_id_product(inputDevice);
            seat = libinput_event_touch_get_seat_slot(event);
            time = libinput_event_touch_get_time(event);

            LargestRawGestureEvent event = {{mask, {id, seat, point, pointPixel, time}}};
            if(mask == TouchStartMask) {
                setRawGestureEventNames(&event, libinput_device_get_sysname(inputDevice), libinput_device_get_name(inputDevice));
            }
            writeTouchEvent(STDOUT_FILENO, &event.event);
            break;
    }
}

static volatile bool isListening = 0;
void stopGestures() {
    isListening = 0;
}

static int listenForGestures(struct libinput* li) {
    int libinput_fd = libinput_get_fd(li);
    struct pollfd fds[2] = {{libinput_fd, POLLIN}, {STDOUT_FILENO, 0}};
    isListening = 1;
    while(isListening) {
        int ret = poll(fds, LEN(fds), -1);
        if (ret == -1) {
            if (errno == EAGAIN || errno == ENOMEM)
                continue;
            return -2;
        }
        for (int i = 0; i < LEN(fds); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == libinput_fd) {
                    if (libinput_dispatch(li)) {
                        return -1;
                    }
                    struct libinput_event* event;
                    while (event = libinput_get_event(li)) {
                        enum libinput_event_type type = libinput_event_get_type(event);
                        processTouchEvent((struct libinput_event_touch*)event, type);
                        libinput_event_destroy(event);
                    }
                }
            } else if(fds[i].revents & (POLLERR | POLLHUP)) {
                isListening = 0;
            }
        }
    }
    return 0;
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
int startGestures(const char** paths, int num, bool grab) {
    struct libinput* li = paths && num ? createPathInterface(paths, num, grab) : createUdevInterface(grab);
    if (li) {
        int ret = listenForGestures(li);
        libinput_unref(li);
        return ret;
    }
    return 1;
}

int __attribute__((weak)) main(int argc, char* const argv[]) {
    bool grab = 0;
    int i = 1;
    if(argv[1] && strcmp(argv[1], "--grab")) {
        i++;
        grab = 1;
    }
    return startGestures((const char**)(argv + 1), argc - i, grab);
}
